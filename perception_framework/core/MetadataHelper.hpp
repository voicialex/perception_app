#pragma once

#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <memory>
#include "libobsensor/ObSensor.hpp"
#include "libobsensor/h/ObTypes.h"
#include "utils.hpp"

/**
 * @brief 元数据处理助手 - 专门处理帧元数据的提取和格式化
 * 现在作为 DumpHelper 的组件使用
 */
class MetadataHelper {
public:
    static MetadataHelper& getInstance();

    // 控制台打印元数据信息（保留原有功能）
    void printMetadata(std::shared_ptr<ob::Frame> frame, int interval);
    
    // 提取元数据为格式化字符串（供 DumpHelper 使用）
    std::string extractMetadataToString(std::shared_ptr<ob::Frame> frame);
    
    // 提取基本帧信息
    std::string extractFrameInfo(std::shared_ptr<ob::Frame> frame);
    
    // 提取设备元数据
    std::string extractDeviceMetadata(std::shared_ptr<ob::Frame> frame);
    
    // 提取特定类型帧的详细信息
    std::string extractVideoFrameInfo(std::shared_ptr<ob::VideoFrame> frame);
    std::string extractDepthFrameInfo(std::shared_ptr<ob::DepthFrame> frame);
    std::string extractPointsFrameInfo(std::shared_ptr<ob::PointsFrame> frame);
    std::string extractIMUFrameInfo(std::shared_ptr<ob::Frame> frame);

private:
    std::string metadataTypeToString(OBFrameMetadataType type);
    std::string formatFrameInfo(std::shared_ptr<ob::Frame> frame);

private:
    MetadataHelper() = default;
    ~MetadataHelper() = default;
    MetadataHelper(const MetadataHelper&) = delete;
    MetadataHelper& operator=(const MetadataHelper&) = delete;

    static const char* metaDataTypes[];
};
