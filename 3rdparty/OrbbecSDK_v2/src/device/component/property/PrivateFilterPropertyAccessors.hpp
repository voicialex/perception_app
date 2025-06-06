// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "IProperty.hpp"
#include "IDevice.hpp"

namespace libobsensor {

class PrivateFilterPropertyAccessor : public IBasicPropertyAccessor {
public:
    PrivateFilterPropertyAccessor(IDevice *device);
    virtual ~PrivateFilterPropertyAccessor() noexcept override = default;

    void setPropertyValue(uint32_t propertyId, const OBPropertyValue &value) override;
    void getPropertyValue(uint32_t propertyId, OBPropertyValue *value) override;
    void getPropertyRange(uint32_t propertyId, OBPropertyRange *range) override;

private:
    IDevice *device_;
};
}  // namespace libobsensor
