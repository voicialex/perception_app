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

    // 通用图像帧保存方法 - 处理所有类型的图像帧
    bool saveImageFrame(const std::shared_ptr<ob::VideoFrame> videoFrame, 
                       const std::string& path, 
                       const std::string& timeStamp,
                       const std::string& frameTypeName,
                       int cvMatType);

    // 保存深度帧
    void saveDepthFrame(const std::shared_ptr<ob::DepthFrame> depthFrame, 
                       const std::string& path, const std::string& timeStamp);
    
    // 保存彩色帧
    void saveColorFrame(std::shared_ptr<ob::ColorFrame> colorFrame, 
                       const std::string& path, const std::string& timeStamp);
    
    // 保存IR帧
    void saveIRFrame(std::shared_ptr<ob::Frame> irFrame, 
                     const std::string& path, const std::string& timeStamp);
    
    // 保存IMU帧
    void saveIMUFrame(const std::shared_ptr<ob::Frame> frame, 
                     const std::string& path, const std::string& timeStamp);
    
    // 创建文件路径
    std::string createFilePath(const std::string& path, 
                              const std::string& timeStamp, 
                              const std::string& frameTypeName,
                              const std::string& extension = ".png");
    
    // 创建 PNG 保存参数
    std::vector<int> createPNGParams();
    
    // 生成统一的时间戳字符串
    std::string generateTimeStamp();

    // 格式转换器
    std::shared_ptr<ob::FormatConvertFilter> formatConverter_;
}; 