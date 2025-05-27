#pragma once

#include <memory>
#include <string>
#include <opencv2/opencv.hpp>
#include "libobsensor/ObSensor.hpp"

class DumpHelper {
public:
    static DumpHelper& getInstance();

    // 保存帧数据
    void saveFrame(std::shared_ptr<ob::Frame> frame, const std::string& path);

private:
    DumpHelper();
    ~DumpHelper() = default;
    DumpHelper(const DumpHelper&) = delete;
    DumpHelper& operator=(const DumpHelper&) = delete;

    // 保存深度帧
    void saveDepthFrame(const std::shared_ptr<ob::DepthFrame> depthFrame, const std::string& path);
    
    // 保存彩色帧
    void saveColorFrame(std::shared_ptr<ob::ColorFrame> colorFrame, const std::string& path);
    
    // 保存IMU帧
    void saveIMUFrame(const std::shared_ptr<ob::Frame> frame, const std::string& path);

    // 检查是否为视频帧
    bool isVideoFrame(OBFrameType type);
    
    // 检查是否为IMU帧
    bool isIMUFrame(OBFrameType type);

    // 格式转换器
    std::shared_ptr<ob::FormatConvertFilter> formatConverter_;
}; 