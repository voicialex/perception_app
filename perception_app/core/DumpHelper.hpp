#pragma once

#include <memory>
#include <string>
#include <opencv2/opencv.hpp>
#include "libobsensor/ObSensor.hpp"

class DumpHelper {
public:
    static DumpHelper& getInstance();

    // 初始化数据保存路径（内部调用ConfigHelper的ensureDirectoryExists）
    bool initializeSavePath();

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

    // 通用图像保存方法 - 直接保存cv::Mat图像
    bool saveImageFrame(const cv::Mat& imageMat,
                       const std::string& path,
                       const std::string& timeStamp,
                       const std::string& frameTypeName,
                       const std::string& extension = ".png");

    // 保存深度帧
    void saveDepthFrame(const std::shared_ptr<ob::DepthFrame> depthFrame, 
                       const std::string& path);
    
    // 保存深度colormap帧
    void saveDepthColormapFrame(const std::shared_ptr<ob::DepthFrame> depthFrame,
                               const std::string& path);
    
    // 保存彩色帧
    void saveColorFrame(std::shared_ptr<ob::ColorFrame> colorFrame, 
                       const std::string& path);
    
    // 保存IR帧
    void saveIRFrame(std::shared_ptr<ob::Frame> irFrame, 
                     const std::string& path);
    
    // 保存IMU帧
    void saveIMUFrame(const std::shared_ptr<ob::Frame> frame, 
                     const std::string& path);
    
    // 新增：创建深度图像的colormap版本（参考utils_opencv.cpp）
    cv::Mat createDepthColormap(const std::shared_ptr<ob::DepthFrame> depthFrame);
    
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