// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "IFrame.hpp"
#include "IFrameTimestamp.hpp"
#include "IFrameTimestamp.hpp"
#include "DeviceComponentBase.hpp"

namespace libobsensor {
class GlobalTimestampCalculator : public IFrameTimestampCalculator, public DeviceComponentBase {
public:
    GlobalTimestampCalculator(IDevice *device, uint64_t deviceTimeFreq, uint64_t frameTimeFreq);

    virtual ~GlobalTimestampCalculator() noexcept override = default;

    void calculate(std::shared_ptr<Frame> frame) override;
    void clear() override;

private:
    uint64_t                               deviceTimeFreq_;
    uint64_t                               frameTimeFreq_;
    std::shared_ptr<IGlobalTimestampFitter> globalTimestampFitter_;
};

class FrameTimestampCalculatorDirectly : public IFrameTimestampCalculator, public DeviceComponentBase {
public:
    FrameTimestampCalculatorDirectly(IDevice *device, uint64_t clockFreq);

    virtual ~FrameTimestampCalculatorDirectly() noexcept override = default;

    void calculate(std::shared_ptr<Frame> frame) override;
    void clear() override {}

private:
    uint64_t clockFreq_;
};

class FrameTimestampCalculatorBaseDeviceTime : public IFrameTimestampCalculator, public DeviceComponentBase {
public:
    FrameTimestampCalculatorBaseDeviceTime(IDevice *device, uint64_t deviceTimeFreq, uint64_t frameTimeFreq);

    virtual ~FrameTimestampCalculatorBaseDeviceTime() noexcept override = default;

    void calculate(std::shared_ptr<Frame> frame) override;
    void clear() override;

    uint64_t calculate(uint64_t srcTimestamp);

private:
    uint64_t deviceTimeFreq_;
    uint64_t frameTimeFreq_;

    uint64_t prevSrcTsp_;
    uint64_t prevHostTsp_;
    uint64_t baseDevTime_;
};

class FrameTimestampCalculatorOverMetadata : public IFrameTimestampCalculator, public DeviceComponentBase {
public:
    FrameTimestampCalculatorOverMetadata(IDevice *device, OBFrameMetadataType metadataType, uint64_t clockFreq);

    virtual ~FrameTimestampCalculatorOverMetadata() noexcept override = default;

    void calculate(std::shared_ptr<Frame> frame) override;
    void clear() override;

private:
    OBFrameMetadataType metadataType_;
    uint64_t            clockFreq_;
};

class FrameTimestampCalculatorOverUvcSCR : public IFrameTimestampCalculator, public DeviceComponentBase {
public:
    FrameTimestampCalculatorOverUvcSCR(IDevice *device, uint64_t clockFreq);

    virtual ~FrameTimestampCalculatorOverUvcSCR() noexcept override = default;

    void calculate(std::shared_ptr<Frame> frame) override;
    void clear() override;

private:
    uint64_t clockFreq_;
};

class G435LeFrameTimestampCalculatorDeviceTime : public IFrameTimestampCalculator, public DeviceComponentBase{
    public:
    G435LeFrameTimestampCalculatorDeviceTime(IDevice *device, uint64_t deviceTimeFreq, uint64_t frameTimeFreq, uint64_t clockFreq);
    
        virtual ~G435LeFrameTimestampCalculatorDeviceTime() = default;
    
        void calculate(std::shared_ptr<Frame> frame) override;

        void clear() override;
    
    private:
        std::shared_ptr<FrameTimestampCalculatorBaseDeviceTime> baseCalculator_;
        std::shared_ptr<FrameTimestampCalculatorDirectly>      directCalculator_;

};

}  // namespace libobsensor
