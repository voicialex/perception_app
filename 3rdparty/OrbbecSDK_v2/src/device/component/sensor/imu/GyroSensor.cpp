// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "GyroSensor.hpp"
#include "IDevice.hpp"
#include "property/InternalProperty.hpp"
#include "stream/StreamProfileFactory.hpp"

namespace libobsensor {

#pragma pack(1)
typedef struct {
    uint32_t         num;
    OBGyroSampleRate odr[16];
} GyroSampleRateList;

typedef struct {
    uint32_t             num;
    OBGyroFullScaleRange fs[16];
} GyroFullScaleRangeList;
#pragma pack()

GyroSensor::GyroSensor(IDevice *owner, const std::shared_ptr<ISourcePort> &backend, const std::shared_ptr<IStreamer> &streamer)
    : SensorBase(owner, OB_SENSOR_GYRO, backend), streamer_(streamer) {
    auto propServer = owner->getPropertyServer();

    auto gyroSampleRateList     = propServer->getStructureDataT<GyroSampleRateList>(OB_STRUCT_GET_GYRO_PRESETS_ODR_LIST);
    auto gyroFullScaleRangeList = propServer->getStructureDataT<GyroFullScaleRangeList>(OB_STRUCT_GET_GYRO_PRESETS_FULL_SCALE_LIST);

    auto lazySensor = std::make_shared<LazySensor>(owner, OB_SENSOR_GYRO);
    for(uint32_t i = 0; i < gyroSampleRateList.num; i++) {
        for(uint32_t j = 0; j < gyroFullScaleRangeList.num; j++) {
            auto gyro_odr = gyroSampleRateList.odr[i];
            auto gyro_fs  = gyroFullScaleRangeList.fs[j];
            if(gyro_odr == 0 || gyro_fs == 0) {
                continue;
            }

            auto profile = StreamProfileFactory::createGyroStreamProfile(lazySensor, gyro_fs, gyro_odr);
            streamProfileList_.emplace_back(profile);
        }
    }

    if(propServer->isPropertySupported(OB_PROP_GYRO_ODR_INT, PROP_OP_READ, PROP_ACCESS_INTERNAL)
       && propServer->isPropertySupported(OB_PROP_GYRO_FULL_SCALE_INT, PROP_OP_READ, PROP_ACCESS_INTERNAL)) {
        auto currentSampleRate = propServer->getPropertyValueT<OBGyroSampleRate>(OB_PROP_GYRO_ODR_INT);
        auto currentFullScale  = propServer->getPropertyValueT<OBGyroFullScaleRange>(OB_PROP_GYRO_FULL_SCALE_INT);
        auto defaultProfile    = StreamProfileFactory::createGyroStreamProfile(lazySensor, currentFullScale, currentSampleRate);
        updateDefaultStreamProfile(defaultProfile);
    }

    LOG_DEBUG("GyroSensor is created!");
}

GyroSensor::~GyroSensor() noexcept {
    if(isStreamActivated()) {
        TRY_EXECUTE(stop());
    }
    LOG_DEBUG("GyroSensor is destroyed");
}

void GyroSensor::start(std::shared_ptr<const StreamProfile> sp, FrameCallback callback) {
    activatedStreamProfile_ = sp;
    frameCallback_          = callback;
    updateStreamState(STREAM_STATE_STARTING);

    auto owner      = getOwner();
    auto propServer = owner->getPropertyServer();

    auto gyroSp = sp->as<GyroStreamProfile>();
    propServer->setPropertyValueT(OB_PROP_GYRO_ODR_INT, static_cast<int>(gyroSp->getSampleRate()));
    propServer->setPropertyValueT(OB_PROP_GYRO_FULL_SCALE_INT, static_cast<int>(gyroSp->getFullScaleRange()));
    propServer->setPropertyValueT(OB_PROP_GYRO_SWITCH_BOOL, true);

    BEGIN_TRY_EXECUTE({
        streamer_->startStream(sp, [this](std::shared_ptr<Frame> frame) {
            if(streamState_ != STREAM_STATE_STREAMING && streamState_ != STREAM_STATE_STARTING) {
                return;
            }

            updateStreamState(STREAM_STATE_STREAMING);
            outputFrame(frame);
        });
    })
    CATCH_EXCEPTION_AND_EXECUTE({
        activatedStreamProfile_.reset();
        frameCallback_ = nullptr;
        updateStreamState(STREAM_STATE_START_FAILED);
        throw;
    })
}

void GyroSensor::stop() {
    updateStreamState(STREAM_STATE_STOPPING);
    streamer_->stopStream(activatedStreamProfile_);
    auto owner      = getOwner();
    auto propServer = owner->getPropertyServer();
    propServer->setPropertyValueT(OB_PROP_GYRO_SWITCH_BOOL, false);
    updateStreamState(STREAM_STATE_STOPPED);
}

}  // namespace libobsensor
