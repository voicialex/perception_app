// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "FemtoMegaIDevice.hpp"
#include "environment/EnvConfig.hpp"
#include "stream/StreamProfileFactory.hpp"
#include "sensor/video/VideoSensor.hpp"
#include "sensor/video/DisparityBasedSensor.hpp"
#include "sensor/imu/ImuStreamer.hpp"
#include "sensor/imu/AccelSensor.hpp"
#include "sensor/imu/GyroSensor.hpp"
#include "usb/uvc/UvcDevicePort.hpp"

#include "metadata/FrameMetadataParserContainer.hpp"
#include "timestamp/GlobalTimestampFitter.hpp"
#include "timestamp/FrameTimestampCalculator.hpp"
#include "timestamp/DeviceClockSynchronizer.hpp"
#include "property/VendorPropertyAccessor.hpp"
#include "property/UvcPropertyAccessor.hpp"
#include "property/PropertyServer.hpp"
#include "property/CommonPropertyAccessors.hpp"
#include "property/FilterPropertyAccessors.hpp"
#include "property/PrivateFilterPropertyAccessors.hpp"
#include "param/AlgParamManager.hpp"
#include "syncconfig/DeviceSyncConfigurator.hpp"

#include "FilterFactory.hpp"
#include "publicfilters/FormatConverterProcess.hpp"
#include "publicfilters/IMUCorrector.hpp"

#include "utils/BufferParser.hpp"
#include "utils/PublicTypeHelper.hpp"

#include "FemtoMegaPropertyAccessor.hpp"
#include "FemtoMegaFrameTimestampCalculator.hpp"
#include "firmwareupdater/FirmwareUpdater.hpp"
#include "monitor/DeviceMonitor.hpp"

#if defined(BUILD_NET_PAL)
#include "ethernet/RTSPStreamPort.hpp"
#endif

namespace libobsensor {
FemtoMegaINetDevice::FemtoMegaINetDevice(const std::shared_ptr<const IDeviceEnumInfo> &info) : DeviceBase(info) {
    init();
}

FemtoMegaINetDevice::~FemtoMegaINetDevice() noexcept {}

void FemtoMegaINetDevice::init() {
    initProperties();

    fetchDeviceInfo();
    fetchExtensionInfo();

    if(!inRecoveryMode_) {
        initSensorList();
        fetchAllVideoStreamProfileList();

        if(getFirmwareVersionInt() >= 10209) {
            deviceTimeFreq_     = 1000000;
            depthFrameTimeFreq_ = 1000000;
        }

        auto globalTimestampFilter = std::make_shared<GlobalTimestampFitter>(this);
        registerComponent(OB_DEV_COMPONENT_GLOBAL_TIMESTAMP_FILTER, globalTimestampFilter);

        auto algParamManager = std::make_shared<TOFDeviceCommonAlgParamManager>(this);
        registerComponent(OB_DEV_COMPONENT_ALG_PARAM_MANAGER, algParamManager);

        static const std::vector<OBMultiDeviceSyncMode> supportedSyncModes = { OB_MULTI_DEVICE_SYNC_MODE_FREE_RUN, OB_MULTI_DEVICE_SYNC_MODE_STANDALONE,
                                                                               OB_MULTI_DEVICE_SYNC_MODE_PRIMARY, OB_MULTI_DEVICE_SYNC_MODE_SECONDARY,
                                                                               OB_MULTI_DEVICE_SYNC_MODE_SOFTWARE_TRIGGERING };
        static const std::map<OBMultiDeviceSyncMode, OBSyncMode> syncModeNewToOldMap = {
            { OB_MULTI_DEVICE_SYNC_MODE_FREE_RUN, OB_SYNC_MODE_CLOSE },
            { OB_MULTI_DEVICE_SYNC_MODE_STANDALONE, OB_SYNC_MODE_STANDALONE },
            { OB_MULTI_DEVICE_SYNC_MODE_PRIMARY, OB_SYNC_MODE_PRIMARY_MCU_TRIGGER },
            { OB_MULTI_DEVICE_SYNC_MODE_SECONDARY, OB_SYNC_MODE_SECONDARY },
            { OB_MULTI_DEVICE_SYNC_MODE_SECONDARY_SYNCED, OB_SYNC_MODE_SECONDARY },
            { OB_MULTI_DEVICE_SYNC_MODE_SOFTWARE_TRIGGERING, OB_SYNC_MODE_PRIMARY_SOFT_TRIGGER }
        };
        static const std::map<OBSyncMode, OBMultiDeviceSyncMode> syncModeOldToNewMap = {
            { OB_SYNC_MODE_CLOSE, OB_MULTI_DEVICE_SYNC_MODE_FREE_RUN },
            { OB_SYNC_MODE_STANDALONE, OB_MULTI_DEVICE_SYNC_MODE_STANDALONE },
            { OB_SYNC_MODE_PRIMARY_MCU_TRIGGER, OB_MULTI_DEVICE_SYNC_MODE_PRIMARY },
            { OB_SYNC_MODE_SECONDARY, OB_MULTI_DEVICE_SYNC_MODE_SECONDARY },
            { OB_SYNC_MODE_PRIMARY_SOFT_TRIGGER, OB_MULTI_DEVICE_SYNC_MODE_SOFTWARE_TRIGGERING }
        };
        auto deviceSyncConfigurator = std::make_shared<DeviceSyncConfiguratorOldProtocol>(this, supportedSyncModes);
        deviceSyncConfigurator->updateModeAliasMap(syncModeOldToNewMap, syncModeNewToOldMap);
        deviceSyncConfigurator->enableDepthDelaySupport(true);
        registerComponent(OB_DEV_COMPONENT_DEVICE_SYNC_CONFIGURATOR, deviceSyncConfigurator);

        auto deviceClockSynchronizer = std::make_shared<DeviceClockSynchronizer>(this, deviceTimeFreq_, deviceTimeFreq_);
        registerComponent(OB_DEV_COMPONENT_DEVICE_CLOCK_SYNCHRONIZER, deviceClockSynchronizer);
    }
    // firmware updater
    registerComponent(OB_DEV_COMPONENT_FIRMWARE_UPDATER, [this]() {
        std::shared_ptr<FirmwareUpdater> firmwareUpdater;
        TRY_EXECUTE({ firmwareUpdater = std::make_shared<FirmwareUpdater>(this); })
        return firmwareUpdater;
    });
}

void FemtoMegaINetDevice::fetchDeviceInfo() {
    auto portInfo          = enumInfo_->getSourcePortInfoList().front();
    auto netPortInfo       = std::dynamic_pointer_cast<const NetSourcePortInfo>(portInfo);
    auto deviceInfo        = std::make_shared<NetDeviceInfo>();
    deviceInfo->ipAddress_ = netPortInfo->address;
    deviceInfo_            = deviceInfo;

    deviceInfo_->name_           = enumInfo_->getName();
    deviceInfo_->pid_            = enumInfo_->getPid();
    deviceInfo_->vid_            = enumInfo_->getVid();
    deviceInfo_->uid_            = enumInfo_->getUid();
    deviceInfo_->connectionType_ = enumInfo_->getConnectionType();

    auto propServer                   = getPropertyServer();
    auto version                      = propServer->getStructureDataT<OBVersionInfo>(OB_STRUCT_VERSION);
    deviceInfo_->fwVersion_           = version.firmwareVersion;
    deviceInfo_->deviceSn_            = version.serialNumber;
    deviceInfo_->asicName_            = version.depthChip;
    deviceInfo_->hwVersion_           = version.hardwareVersion;
    deviceInfo_->type_                = static_cast<uint16_t>(version.deviceType);
    deviceInfo_->supportedSdkVersion_ = version.sdkVersion;

    // remove the prefix "Orbbec " from the device name if contained
    if(deviceInfo_->name_.find("Orbbec ") == 0) {
        deviceInfo_->name_ = deviceInfo_->name_.substr(7);
    }
    deviceInfo_->fullName_ = "Orbbec " + deviceInfo_->name_;

    // mark the device as a multi-sensor device with same clock at default
    extensionInfo_["AllSensorsUsingSameClock"] = "true";
}

void FemtoMegaINetDevice::initSensorList() {
    registerComponent(OB_DEV_COMPONENT_FRAME_PROCESSOR_FACTORY, [this]() {
        std::shared_ptr<FrameProcessorFactory> factory;
        TRY_EXECUTE({ factory = std::make_shared<FrameProcessorFactory>(this); })
        return factory;
    });

    const auto &sourcePortInfoList = enumInfo_->getSourcePortInfoList();
    auto depthPortInfoIter = std::find_if(sourcePortInfoList.begin(), sourcePortInfoList.end(), [](const std::shared_ptr<const SourcePortInfo> &portInfo) {
        return portInfo->portType == SOURCE_PORT_NET_RTSP && std::dynamic_pointer_cast<const RTSPStreamPortInfo>(portInfo)->streamType == OB_STREAM_DEPTH;
    });

    if(depthPortInfoIter != sourcePortInfoList.end()) {
        auto depthPortInfo = *depthPortInfoIter;
        registerComponent(
            OB_DEV_COMPONENT_DEPTH_SENSOR,
            [this, depthPortInfo]() {
                auto port = getSourcePort(depthPortInfo);

                auto sensor = std::make_shared<VideoSensor>(this, OB_SENSOR_DEPTH, port);

                auto videoFrameTimestampCalculator_ = std::make_shared<FrameTimestampCalculatorDirectly>(this, depthFrameTimeFreq_);
                sensor->setFrameTimestampCalculator(videoFrameTimestampCalculator_);

                auto globalFrameTimestampCalculator = std::make_shared<GlobalTimestampCalculator>(this, deviceTimeFreq_, depthFrameTimeFreq_);
                sensor->setGlobalTimestampCalculator(globalFrameTimestampCalculator);

                auto frameProcessor = getComponentT<FrameProcessor>(OB_DEV_COMPONENT_DEPTH_FRAME_PROCESSOR, false);
                if(frameProcessor) {
                    sensor->setFrameProcessor(frameProcessor.get());
                }

                initSensorStreamProfile(sensor);
                return sensor;
            },
            true);

        registerSensorPortInfo(OB_SENSOR_DEPTH, depthPortInfo);

        registerComponent(OB_DEV_COMPONENT_DEPTH_FRAME_PROCESSOR, [this]() {
            auto factory = getComponentT<FrameProcessorFactory>(OB_DEV_COMPONENT_FRAME_PROCESSOR_FACTORY);

            auto frameProcessor = factory->createFrameProcessor(OB_SENSOR_DEPTH);
            return frameProcessor;
        });
    }

    auto irPortInfoIter = std::find_if(sourcePortInfoList.begin(), sourcePortInfoList.end(), [](const std::shared_ptr<const SourcePortInfo> &portInfo) {
        return portInfo->portType == SOURCE_PORT_NET_RTSP && std::dynamic_pointer_cast<const RTSPStreamPortInfo>(portInfo)->streamType == OB_STREAM_IR;
    });

    if(irPortInfoIter != sourcePortInfoList.end()) {
        auto irPortInfo = *irPortInfoIter;

        registerComponent(
            OB_DEV_COMPONENT_IR_SENSOR,
            [this, irPortInfo]() {
                auto port = getSourcePort(irPortInfo);

                auto sensor = std::make_shared<VideoSensor>(this, OB_SENSOR_IR, port);

                auto videoFrameTimestampCalculator_ = std::make_shared<FrameTimestampCalculatorDirectly>(this, depthFrameTimeFreq_);
                sensor->setFrameTimestampCalculator(videoFrameTimestampCalculator_);

                auto globalFrameTimestampCalculator = std::make_shared<GlobalTimestampCalculator>(this, deviceTimeFreq_, depthFrameTimeFreq_);
                sensor->setGlobalTimestampCalculator(globalFrameTimestampCalculator);

                initSensorStreamProfile(sensor);
                return sensor;
            },
            true);
        registerSensorPortInfo(OB_SENSOR_IR, irPortInfo);
        registerComponent(OB_DEV_COMPONENT_IR_FRAME_PROCESSOR, [this]() {
            auto factory = getComponentT<FrameProcessorFactory>(OB_DEV_COMPONENT_FRAME_PROCESSOR_FACTORY);

            auto frameProcessor = factory->createFrameProcessor(OB_SENSOR_IR);
            return frameProcessor;
        });
    }

    auto colorPortInfoIter = std::find_if(sourcePortInfoList.begin(), sourcePortInfoList.end(), [](const std::shared_ptr<const SourcePortInfo> &portInfo) {
        return portInfo->portType == SOURCE_PORT_NET_RTSP && std::dynamic_pointer_cast<const RTSPStreamPortInfo>(portInfo)->streamType == OB_STREAM_COLOR;
    });

    if(colorPortInfoIter != sourcePortInfoList.end()) {
        auto colorPortInfo = *colorPortInfoIter;
        registerComponent(
            OB_DEV_COMPONENT_COLOR_SENSOR,
            [this, colorPortInfo]() {
                auto port   = getSourcePort(colorPortInfo);
                auto sensor = std::make_shared<VideoSensor>(this, OB_SENSOR_COLOR, port);

                std::vector<FormatFilterConfig> formatFilterConfigs = {
                    { FormatFilterPolicy::REMOVE, OB_FORMAT_NV12, OB_FORMAT_ANY, nullptr },
                };
                auto formatConverter = getSensorFrameFilter("FormatConverter", OB_SENSOR_COLOR, false);
                if(formatConverter) {
                    formatFilterConfigs.push_back({ FormatFilterPolicy::ADD, OB_FORMAT_MJPG, OB_FORMAT_RGB, formatConverter });
                    formatFilterConfigs.push_back({ FormatFilterPolicy::ADD, OB_FORMAT_MJPG, OB_FORMAT_BGR, formatConverter });
                    formatFilterConfigs.push_back({ FormatFilterPolicy::ADD, OB_FORMAT_MJPG, OB_FORMAT_BGRA, formatConverter });
                }
                sensor->updateFormatFilterConfig(formatFilterConfigs);

                if(getFirmwareVersionInt() >= 20003) {
                    auto videoFrameTimestampCalculator_ =
                        std::make_shared<FemtoMegaColorFrameTimestampCalculatorV10300>(this, deviceTimeFreq_, colorFrameTimeFreq_);
                    sensor->setFrameTimestampCalculator(videoFrameTimestampCalculator_);
                }
                else {
                    auto videoFrameTimestampCalculator_ = std::make_shared<FrameTimestampCalculatorBaseDeviceTime>(this, deviceTimeFreq_, colorFrameTimeFreq_);
                    sensor->setFrameTimestampCalculator(videoFrameTimestampCalculator_);
                }

                auto globalFrameTimestampCalculator = std::make_shared<GlobalTimestampCalculator>(this, deviceTimeFreq_, colorFrameTimeFreq_);
                sensor->setGlobalTimestampCalculator(globalFrameTimestampCalculator);

                auto frameProcessor = getComponentT<FrameProcessor>(OB_DEV_COMPONENT_COLOR_FRAME_PROCESSOR, false);
                if(frameProcessor) {
                    sensor->setFrameProcessor(frameProcessor.get());
                }

                initSensorStreamProfile(sensor);

                return sensor;
            },
            true);
        registerSensorPortInfo(OB_SENSOR_COLOR, colorPortInfo);
        registerComponent(OB_DEV_COMPONENT_COLOR_FRAME_PROCESSOR, [this]() {
            auto factory        = getComponentT<FrameProcessorFactory>(OB_DEV_COMPONENT_FRAME_PROCESSOR_FACTORY);
            auto frameProcessor = factory->createFrameProcessor(OB_SENSOR_COLOR);
            return frameProcessor;
        });
    }

    auto imuPortInfoIter = std::find_if(sourcePortInfoList.begin(), sourcePortInfoList.end(), [](const std::shared_ptr<const SourcePortInfo> &portInfo) {
        return portInfo->portType == SOURCE_PORT_NET_VENDOR_STREAM;
    });

    if(imuPortInfoIter != sourcePortInfoList.end()) {
        auto imuPortInfo = *imuPortInfoIter;
        registerComponent(OB_DEV_COMPONENT_IMU_STREAMER, [this, imuPortInfo]() {
            // the gyro and accel are both on the same port and share the same filter
            auto                                  port               = getSourcePort(imuPortInfo);
            auto                                  imuReversionFilter = getSensorFrameFilter("IMUFrameReversion", OB_SENSOR_ACCEL, true);
            auto                                  imuCorrectorFilter = getSensorFrameFilter("IMUCorrector", OB_SENSOR_ACCEL, true);
            std::vector<std::shared_ptr<IFilter>> imuFilters         = { imuReversionFilter, imuCorrectorFilter };
            auto                                  dataStreamPort     = std::dynamic_pointer_cast<IDataStreamPort>(port);
            auto                                  imuStreamer        = std::make_shared<ImuStreamer>(this, dataStreamPort, imuFilters);
            return imuStreamer;
        });

        registerComponent(
            OB_DEV_COMPONENT_ACCEL_SENSOR,
            [this, imuPortInfo]() {
                auto port                 = getSourcePort(imuPortInfo);
                auto imuStreamer          = getComponentT<ImuStreamer>(OB_DEV_COMPONENT_IMU_STREAMER);
                auto imuStreamerSharedPtr = imuStreamer.get();
                auto sensor               = std::make_shared<AccelSensor>(this, port, imuStreamerSharedPtr);

                auto globalFrameTimestampCalculator = std::make_shared<GlobalTimestampCalculator>(this, deviceTimeFreq_, depthFrameTimeFreq_);
                sensor->setGlobalTimestampCalculator(globalFrameTimestampCalculator);

                initSensorStreamProfile(sensor);
                return sensor;
            },
            true);
        registerSensorPortInfo(OB_SENSOR_ACCEL, imuPortInfo);

        registerComponent(
            OB_DEV_COMPONENT_GYRO_SENSOR,
            [this, imuPortInfo]() {
                auto port                 = getSourcePort(imuPortInfo);
                auto imuStreamer          = getComponentT<ImuStreamer>(OB_DEV_COMPONENT_IMU_STREAMER);
                auto imuStreamerSharedPtr = imuStreamer.get();
                auto sensor               = std::make_shared<GyroSensor>(this, port, imuStreamerSharedPtr);

                auto globalFrameTimestampCalculator = std::make_shared<GlobalTimestampCalculator>(this, deviceTimeFreq_, depthFrameTimeFreq_);
                sensor->setGlobalTimestampCalculator(globalFrameTimestampCalculator);

                initSensorStreamProfile(sensor);

                return sensor;
            },
            true);
        registerSensorPortInfo(OB_SENSOR_GYRO, imuPortInfo);
    }
}

void FemtoMegaINetDevice::initProperties() {
    const auto &sourcePortInfoList = enumInfo_->getSourcePortInfoList();
    auto        vendorPortInfoIter = std::find_if(sourcePortInfoList.begin(), sourcePortInfoList.end(),
                                                  [](const std::shared_ptr<const SourcePortInfo> &portInfo) { return portInfo->portType == SOURCE_PORT_NET_VENDOR; });

    if(vendorPortInfoIter == sourcePortInfoList.end()) {
        return;
    }

    auto propertyServer = std::make_shared<PropertyServer>(this);

    auto vendorPortInfo         = *vendorPortInfoIter;
    auto vendorPropertyAccessor = std::make_shared<LazySuperPropertyAccessor>([this, vendorPortInfo]() {
        auto port     = getSourcePort(vendorPortInfo);
        auto accessor = std::make_shared<VendorPropertyAccessor>(this, port);
        return accessor;
    });

    // the main property accessor
    registerComponent(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR, [this, vendorPortInfo]() {
        auto port     = getSourcePort(vendorPortInfo);
        auto accessor = std::make_shared<VendorPropertyAccessor>(this, port);
        return accessor;
    });

    registerComponent(OB_DEV_COMPONENT_DEVICE_MONITOR, [this, vendorPortInfo]() {
        auto port       = getSourcePort(vendorPortInfo);
        auto devMonitor = std::make_shared<DeviceMonitor>(this, port);
        return devMonitor;
    });

    auto privatePropertyAccessor = std::make_shared<PrivateFilterPropertyAccessor>(this);
    propertyServer->registerProperty(OB_PROP_DEPTH_SOFT_FILTER_BOOL, "rw", "rw", privatePropertyAccessor);
    propertyServer->registerProperty(OB_PROP_DEPTH_MAX_DIFF_INT, "rw", "rw", privatePropertyAccessor);
    propertyServer->registerProperty(OB_PROP_DEPTH_MAX_SPECKLE_SIZE_INT, "rw", "rw", privatePropertyAccessor);

    propertyServer->registerProperty(OB_PROP_COLOR_AUTO_EXPOSURE_BOOL, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_COLOR_EXPOSURE_INT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_COLOR_GAIN_INT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_COLOR_SATURATION_INT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_COLOR_AUTO_WHITE_BALANCE_BOOL, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_COLOR_WHITE_BALANCE_INT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_COLOR_BRIGHTNESS_INT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_COLOR_SHARPNESS_INT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_COLOR_CONTRAST_INT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_COLOR_POWER_LINE_FREQUENCY_INT, "rw", "rw", vendorPropertyAccessor);

    propertyServer->registerProperty(OB_PROP_DEPTH_ALIGN_HARDWARE_BOOL, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_DEPTH_ALIGN_HARDWARE_MODE_INT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_TIMESTAMP_OFFSET_INT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_EXTERNAL_SIGNAL_RESET_BOOL, "", "rw", vendorPropertyAccessor);
    auto heartbeatPropertyAccessor = std::make_shared<HeartbeatPropertyAccessor>(this);
    propertyServer->registerProperty(OB_PROP_HEARTBEAT_BOOL, "rw", "rw", heartbeatPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_DEVICE_COMMUNICATION_TYPE_INT, "", "w", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_SWITCH_IR_MODE_INT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_FAN_WORK_LEVEL_INT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_FAN_WORK_SPEED_INT, "rw", "rw", vendorPropertyAccessor);
    // propertyServer->registerProperty(OB_PROP_USB_POWER_STATE_INT, "r", "r", vendorPropertyAccessor);
    // propertyServer->registerProperty(OB_PROP_DC_POWER_STATE_INT, "r", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_STRUCT_VERSION, "", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_STRUCT_DEVICE_SERIAL_NUMBER, "r", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_STRUCT_MULTI_DEVICE_SYNC_CONFIG, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_STRUCT_DEVICE_IP_ADDR_CONFIG, "rw", "rw", vendorPropertyAccessor);
    //  propertyServer->registerProperty(OB_STRUCT_LED_CONTROL, "", "w", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_RAW_DATA_CAMERA_CALIB_JSON_FILE, "r", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_STRUCT_DEVICE_TIME, "rw", "rw", vendorPropertyAccessor);
    // propertyServer->registerProperty(OB_RAW_DATA_MCU_UPGRADE_FILE, "rw", "rw", vendorPropertyPort);
    // propertyServer->registerProperty(OB_RAW_DATA_HARDWARE_ALIGN_PARAM, "rw", "rw", vendorPropertyPort);
    propertyServer->registerProperty(OB_PROP_BOOT_INTO_RECOVERY_MODE_BOOL, "w", "w", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_TIMER_RESET_ENABLE_BOOL, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_TIMER_RESET_SIGNAL_BOOL, "w", "w", vendorPropertyAccessor);

    propertyServer->registerProperty(OB_RAW_DATA_IMU_CALIB_PARAM, "", "rw", vendorPropertyAccessor);
    // propertyServer->registerProperty(OB_STRUCT_LED_CONTROL, "", "w", vendorPropertyPort);

    propertyServer->registerProperty(OB_PROP_GYRO_ODR_INT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_ACCEL_ODR_INT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_GYRO_FULL_SCALE_INT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_ACCEL_FULL_SCALE_INT, "rw", "rw", vendorPropertyAccessor);

    propertyServer->registerProperty(OB_STRUCT_GET_ACCEL_PRESETS_ODR_LIST, "", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_STRUCT_GET_ACCEL_PRESETS_FULL_SCALE_LIST, "", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_STRUCT_GET_GYRO_PRESETS_ODR_LIST, "", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_STRUCT_GET_GYRO_PRESETS_FULL_SCALE_LIST, "", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_ACCEL_SWITCH_BOOL, "", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_GYRO_SWITCH_BOOL, "", "rw", vendorPropertyAccessor);

    propertyServer->registerProperty(OB_RAW_DATA_STREAM_PROFILE_LIST, "r", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_RAW_DATA_ALIGN_CALIB_PARAM, "", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_RAW_DATA_D2C_ALIGN_SUPPORT_PROFILE_LIST, "", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_TOF_EXPOSURE_TIME_INT, "r", "r", vendorPropertyAccessor);

    propertyServer->registerProperty(OB_PROP_STOP_IR_STREAM_BOOL, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_STOP_COLOR_STREAM_BOOL, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_STOP_DEPTH_STREAM_BOOL, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_REBOOT_DEVICE_BOOL, "w", "w", vendorPropertyAccessor);

    propertyServer->registerProperty(OB_RAW_DATA_DEVICE_UPGRADE, "", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_STRUCT_DEVICE_UPGRADE_STATUS, "", "rw", vendorPropertyAccessor);

    propertyServer->registerProperty(OB_PROP_DEVICE_IN_RECOVERY_MODE_BOOL, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_DEVICE_DEVELOPMENT_MODE_INT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_STRUCT_DEVICE_STATIC_IP_CONFIG_RECORD, "rw", "rw", vendorPropertyAccessor);

    propertyServer->aliasProperty(OB_PROP_IR_EXPOSURE_INT, OB_PROP_TOF_EXPOSURE_TIME_INT);
    propertyServer->aliasProperty(OB_PROP_DEPTH_EXPOSURE_INT, OB_PROP_TOF_EXPOSURE_TIME_INT);
    propertyServer->registerProperty(OB_PROP_CAPTURE_IMAGE_SIGNAL_BOOL, "w", "w", vendorPropertyAccessor);

    propertyServer->registerProperty(OB_PROP_RESTORE_FACTORY_SETTINGS_BOOL, "w", "w", vendorPropertyAccessor);

    auto imuCorrectorFilter = getSensorFrameFilter("IMUCorrector", OB_SENSOR_ACCEL);
    if(imuCorrectorFilter) {
        auto filterStateProperty = std::make_shared<FilterStatePropertyAccessor>(imuCorrectorFilter);
        propertyServer->registerProperty(OB_PROP_SDK_ACCEL_FRAME_TRANSFORMED_BOOL, "rw", "rw", filterStateProperty);
        propertyServer->registerProperty(OB_PROP_SDK_GYRO_FRAME_TRANSFORMED_BOOL, "rw", "rw", filterStateProperty);
    }

    auto femtoMegaTempPropertyAccessor = std::make_shared<FemtoMegaTempPropertyAccessor>(this);
    propertyServer->registerProperty(OB_STRUCT_DEVICE_TEMPERATURE, "r", "r", femtoMegaTempPropertyAccessor);
    registerComponent(OB_DEV_COMPONENT_PROPERTY_SERVER, propertyServer, true);

    BEGIN_TRY_EXECUTE({ propertyServer->setPropertyValueT(OB_PROP_DEVICE_COMMUNICATION_TYPE_INT, OB_COMM_NET); })
    CATCH_EXCEPTION_AND_EXECUTE({ LOG_ERROR("Set device communication type to ethernet mode failed!"); })

    // check recovery mode
    inRecoveryMode_ = false;
    BEGIN_TRY_EXECUTE({
        inRecoveryMode_ = propertyServer->getPropertyValueT<bool>(OB_PROP_DEVICE_IN_RECOVERY_MODE_BOOL);
        if(inRecoveryMode_) {
            // device is in recovery mode
            LOG_DEBUG("Femto Mega I net device is in recovery mode, skip init ...");
            propertyServer->unregisterAllProperties();
            // only support these properties
            propertyServer->registerProperty(OB_STRUCT_VERSION, "", "r", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_BOOT_INTO_RECOVERY_MODE_BOOL, "w", "w", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_DEVICE_IN_RECOVERY_MODE_BOOL, "r", "r", vendorPropertyAccessor);
            return;
        }
    })
    CATCH_EXCEPTION_AND_EXECUTE({ LOG_ERROR("Get device in recovery mode failed!"); })
}

void FemtoMegaINetDevice::initSensorStreamProfile(std::shared_ptr<ISensor> sensor) {
    auto              sensorType = sensor->getSensorType();
    OBStreamType      streamType = utils::mapSensorTypeToStreamType(sensorType);
    StreamProfileList ProfileList;
    for(const auto &profile: allVideoStreamProfileList_) {
        if(streamType == profile->getType()) {
            ProfileList.push_back(profile);
        }
    }

    if(ProfileList.size() != 0) {
        sensor->setStreamProfileList(ProfileList);
    }

    auto streamProfile = StreamProfileFactory::getDefaultStreamProfileFromEnvConfig(deviceInfo_->name_, sensorType);
    if(streamProfile) {
        sensor->updateDefaultStreamProfile(streamProfile);
    }

    // bind params: extrinsics, intrinsics, etc.
    auto profiles = sensor->getStreamProfileList();
    {
        auto algParamManager = getComponentT<TOFDeviceCommonAlgParamManager>(OB_DEV_COMPONENT_ALG_PARAM_MANAGER);
        algParamManager->bindStreamProfileParams(profiles);
    }

    LOG_INFO("Sensor {} created! Found {} stream profiles.", sensorType, profiles.size());
    for(auto &profile: profiles) {
        LOG_INFO(" - {}", profile);
    }
}

void FemtoMegaINetDevice::fetchAllVideoStreamProfileList() {
    auto propServer = getPropertyServer();

    std::vector<uint8_t> data;
    BEGIN_TRY_EXECUTE({
        propServer->getRawData(
            OB_RAW_DATA_STREAM_PROFILE_LIST,
            [&](OBDataTranState state, OBDataChunk *dataChunk) {
                if(state == DATA_TRAN_STAT_TRANSFERRING) {
                    data.insert(data.end(), dataChunk->data, dataChunk->data + dataChunk->size);
                }
            },
            PROP_ACCESS_INTERNAL);
    })
    CATCH_EXCEPTION_AND_EXECUTE({
        LOG_ERROR("Get profile list params failed!");
        data.clear();
    })

    if(!data.empty()) {
        std::vector<OBInternalStreamProfile> outputProfiles;
        uint16_t                             dataSize = static_cast<uint16_t>(data.size());
        outputProfiles                                = parseBuffer<OBInternalStreamProfile>(data.data(), dataSize);
        allVideoStreamProfileList_.clear();
        for(auto item: outputProfiles) {
            OBStreamType streamType = utils::mapSensorTypeToStreamType((OBSensorType)item.sensorType);
            OBFormat     format     = utils::uvcFourccToOBFormat(item.profile.video.formatFourcc);
            allVideoStreamProfileList_.push_back(StreamProfileFactory::createVideoStreamProfile(streamType, format, item.profile.video.width,
                                                                                                item.profile.video.height, item.profile.video.fps));
        }
    }
    else {
        LOG_WARN("Get stream profile list failed!");
    }
}

std::vector<std::shared_ptr<IFilter>> FemtoMegaINetDevice::createRecommendedPostProcessingFilters(OBSensorType type) {
    auto filterFactory = FilterFactory::getInstance();
    if(type == OB_SENSOR_DEPTH) {
        // activate depth frame processor library
        getComponentT<FrameProcessor>(OB_DEV_COMPONENT_DEPTH_FRAME_PROCESSOR, false);

        std::vector<std::shared_ptr<IFilter>> depthFilterList;

        if(filterFactory->isFilterCreatorExists("DecimationFilter")) {
            auto decimationFilter = filterFactory->createFilter("DecimationFilter");
            depthFilterList.push_back(decimationFilter);
        }

        if(filterFactory->isFilterCreatorExists("SpatialAdvancedFilter")) {
            auto spatFilter = filterFactory->createFilter("SpatialAdvancedFilter");
            // magnitude, alpha, disp_diff, radius
            std::vector<std::string> params = { "1", "0.5", "5", "1" };
            spatFilter->updateConfig(params);
            depthFilterList.push_back(spatFilter);
        }

        if(filterFactory->isFilterCreatorExists("TemporalFilter")) {
            auto tempFilter = filterFactory->createFilter("TemporalFilter");
            // diff_scale, weight
            std::vector<std::string> params = { "0.1", "0.4" };
            tempFilter->updateConfig(params);
            depthFilterList.push_back(tempFilter);
        }

        if(filterFactory->isFilterCreatorExists("HoleFillingFilter")) {
            auto                     hfFilter = filterFactory->createFilter("HoleFillingFilter");
            std::vector<std::string> params   = { "2" };
            hfFilter->updateConfig(params);
            depthFilterList.push_back(hfFilter);
        }

        if(filterFactory->isFilterCreatorExists("ThresholdFilter")) {
            auto ThresholdFilter = filterFactory->createFilter("ThresholdFilter");
            depthFilterList.push_back(ThresholdFilter);
        }

        for(size_t i = 0; i < depthFilterList.size(); i++) {
            auto filter = depthFilterList[i];
            filter->enable(false);
        }
        return depthFilterList;
    }
    else if(type == OB_SENSOR_COLOR) {
        // activate color frame processor library
        getComponentT<FrameProcessor>(OB_DEV_COMPONENT_COLOR_FRAME_PROCESSOR, false);

        std::vector<std::shared_ptr<IFilter>> colorFilterList;
        if(filterFactory->isFilterCreatorExists("DecimationFilter")) {
            auto decimationFilter = filterFactory->createFilter("DecimationFilter");
            decimationFilter->enable(false);
            colorFilterList.push_back(decimationFilter);
        }
        return colorFilterList;
    }

    return {};
}
}  // namespace libobsensor
