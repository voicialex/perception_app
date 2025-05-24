// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "IDevice.hpp"
#include "InternalTypes.hpp"
#include "IDepthWorkModeManager.hpp"
#include "DeviceComponentBase.hpp"

namespace libobsensor {

class G330DepthWorkModeManager : public IDepthWorkModeManager, public DeviceComponentBase {
public:
    G330DepthWorkModeManager(IDevice *owner);
    virtual ~G330DepthWorkModeManager() noexcept override = default;

    std::vector<OBDepthWorkMode_Internal> getDepthWorkModeList() const override;
    const OBDepthWorkMode_Internal &      getCurrentDepthWorkMode() const override;
    void                                  switchDepthWorkMode(const std::string &modeName) override;
    void                                  fetchDepthWorkModeList();

private:
    void switchDepthWorkMode(const OBDepthWorkMode_Internal &targetDepthMode);

private:
    std::vector<OBDepthWorkMode_Internal> depthWorkModeList_;
    OBDepthWorkMode_Internal              currentWorkMode_;
};

}  // namespace libobsensor
