// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "IFilter.hpp"
#include "libobsensor/h/ObTypes.h"
// #include "IProperty.hpp"
#include "InternalTypes.hpp"

namespace libobsensor {

class IMUCorrector : public IFilterBase {
public:
    static OBIMUCalibrateParams parserIMUCalibParamRaw(uint8_t *data, uint32_t size);
    static OBIMUCalibrateParams getDefaultImuCalibParam();

public:
    IMUCorrector();

    virtual ~IMUCorrector() override = default;

    // Config
    void               updateConfig(std::vector<std::string> &params) override;
    const std::string &getConfigSchema() const override;
    void               reset() override {}

private:
    std::shared_ptr<Frame> process(std::shared_ptr<const Frame> frame) override;

    OBAccelValue correctAccel(const OBAccelValue &accelValue, OBAccelIntrinsic *intrinsic);
    OBGyroValue  correctGyro(const OBGyroValue &gyroValue, OBGyroIntrinsic *intrinsic);

protected:
    static constexpr float GYRO_MAX  = 32800 / 0.017453293f;
    static constexpr float ACCEL_MAX = 32768 / 9.80f;
};

}  // namespace libobsensor
