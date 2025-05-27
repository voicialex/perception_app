#pragma once

#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <memory>
#include "libobsensor/ObSensor.hpp"
#include "libobsensor/h/ObTypes.h"
#include "utils.hpp"

class MetadataHelper {
public:
    static MetadataHelper& getInstance();

    // 打印元数据信息
    void printMetadata(std::shared_ptr<ob::Frame> frame, int interval);

private:
    std::string metadataTypeToString(OBFrameMetadataType type);
    const char *frameTypeToString(OBFrameType type);

private:
    MetadataHelper() = default;
    ~MetadataHelper() = default;
    MetadataHelper(const MetadataHelper&) = delete;
    MetadataHelper& operator=(const MetadataHelper&) = delete;

    static const char* metaDataTypes[];
};
