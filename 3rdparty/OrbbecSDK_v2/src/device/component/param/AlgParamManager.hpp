// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "libobsensor/h/ObTypes.h"

#include "IDevice.hpp"
#include "IProperty.hpp"
#include "InternalTypes.hpp"
#include "IAlgParamManager.hpp"

#include "DeviceComponentBase.hpp"

#include <vector>
#include <memory>

namespace libobsensor {
class AlgParamManagerBase : public DeviceComponentBase, public IAlgParamManager {
public:
    AlgParamManagerBase(IDevice *owner);
    virtual ~AlgParamManagerBase() noexcept override = default;

    void bindStreamProfileParams(std::vector<std::shared_ptr<const StreamProfile>> streamProfileList) override;

    const std::vector<OBD2CProfile>  &getD2CProfileList() const override;
    const std::vector<OBCameraParam> &getCalibrationCameraParamList() const override;
    const OBIMUCalibrateParams       &getIMUCalibrationParam() const override;

    virtual void fetchParamFromDevice()    = 0;
    virtual void registerBasicExtrinsics() = 0;

    virtual void bindExtrinsic(std::vector<std::shared_ptr<const StreamProfile>> streamProfileList);
    virtual void bindIntrinsic(std::vector<std::shared_ptr<const StreamProfile>> streamProfileList);

protected:
    // using empty stream profile to initialize and register extrinsic params
    std::vector<std::shared_ptr<const StreamProfile>> basicStreamProfileList_;

    OBIMUCalibrateParams       imuCalibParam_;
    std::vector<OBCameraParam> calibrationCameraParamList_;
    std::vector<OBD2CProfile>  d2cProfileList_;
};

class DisparityAlgParamManagerBase : public AlgParamManagerBase, public IDisparityAlgParamManager {
public:
    DisparityAlgParamManagerBase(IDevice *owner);

    virtual ~DisparityAlgParamManagerBase() noexcept override = default;

    void                    bindStreamProfileParams(std::vector<std::shared_ptr<const StreamProfile>> streamProfileList) override;
    const OBDisparityParam &getDisparityParam() const override;

    virtual void bindDisparityParam(std::vector<std::shared_ptr<const StreamProfile>> streamProfileList);

protected:
    OBDisparityParam disparityParam_;
};

class TOFDeviceCommonAlgParamManager : public AlgParamManagerBase {
public:
    TOFDeviceCommonAlgParamManager(IDevice *owner);
    virtual ~TOFDeviceCommonAlgParamManager() noexcept override = default;

private:
    void fetchParamFromDevice() override;
    void registerBasicExtrinsics() override;
};

}  // namespace libobsensor
