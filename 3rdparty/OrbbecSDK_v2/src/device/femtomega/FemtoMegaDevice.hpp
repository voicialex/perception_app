// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "DeviceBase.hpp"
#include "IDeviceManager.hpp"
#include "frameprocessor/FrameProcessor.hpp"

#include <map>
#include <memory>

namespace libobsensor {
class FemtoMegaUsbDevice : public DeviceBase {
public:
    FemtoMegaUsbDevice(const std::shared_ptr<const IDeviceEnumInfo> &info);
    virtual ~FemtoMegaUsbDevice() noexcept override;
    std::vector<std::shared_ptr<IFilter>> createRecommendedPostProcessingFilters(OBSensorType type) override;

private:
    void init() override;
    void initSensorList();
    void initProperties();
    void initSensorStreamProfile(std::shared_ptr<ISensor> sensor);

private:
    uint64_t deviceTimeFreq_ = 1000;
    uint64_t frameTimeFreq_  = 1000;
};

class FemtoMegaNetDevice : public DeviceBase {
public:
    FemtoMegaNetDevice(const std::shared_ptr<const IDeviceEnumInfo> &info);
    virtual ~FemtoMegaNetDevice() noexcept override;
    std::vector<std::shared_ptr<IFilter>> createRecommendedPostProcessingFilters(OBSensorType type) override;

private:
    void init() override;
    void initSensorList();
    void initProperties();
    void initSensorStreamProfile(std::shared_ptr<ISensor> sensor);
    void fetchAllVideoStreamProfileList();

    void fetchDeviceInfo() override;

private:
    uint64_t deviceTimeFreq_     = 1000;
    uint64_t depthFrameTimeFreq_ = 1000;
    uint64_t colorFrameTimeFreq_ = 90000;

    StreamProfileList allVideoStreamProfileList_;  // fetch from device via vendor-specific protocol for all types of video stream
    bool              inRecoveryMode_ = false;     // whether the device is in recovery mode
};

}  // namespace libobsensor
