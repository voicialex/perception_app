// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "DeviceBase.hpp"
#include "IDeviceManager.hpp"
#include "IFrameTimestamp.hpp"

#include <map>
#include <memory>

namespace libobsensor {

class DabaiADevice : public DeviceBase {
public:
    DabaiADevice(const std::shared_ptr<const IDeviceEnumInfo> &info);
    virtual ~DabaiADevice() noexcept override;

    std::vector<std::shared_ptr<IFilter>> createRecommendedPostProcessingFilters(OBSensorType type) override;

private:
    void init() override;
    void initSensorList();
    void initSensorListGMSL();
    void initProperties();
    void initSensorStreamProfile(std::shared_ptr<ISensor> sensor);

    std::shared_ptr<const StreamProfile> loadDefaultStreamProfile(OBSensorType sensorType);
    void                                 loadDefaultDepthPostProcessingConfig();

private:
    const uint64_t                                              deviceTimeFreq_ = 1000;     // in ms
    const uint64_t                                              frameTimeFreq_  = 1000000;  // in us
    std::function<std::shared_ptr<IFrameTimestampCalculator>()> videoFrameTimestampCalculatorCreator_;
    bool                                                        isGmslDevice_;
};

}  // namespace libobsensor
