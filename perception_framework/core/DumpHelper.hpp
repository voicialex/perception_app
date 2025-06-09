#pragma once

#include <memory>
#include <string>
#include <opencv2/opencv.hpp>
#include "libobsensor/ObSensor.hpp"

// 前向声明
class MetadataHelper;

// 简化的格式转换函数声明
std::string formatName(OBFormat format);
std::string frameTypeName(OBFrameType frameType);

class DumpHelper {
public:
    // 简化：时间戳信息
    struct TimeStamp {
        uint64_t deviceUs;        // 设备时间戳(微秒)
        uint64_t systemUs;        // 系统时间戳(微秒)  
        uint64_t globalUs;        // 全局时间戳(微秒)
        std::string deviceStr;    // 设备时间戳字符串
        std::string systemStr;    // 系统时间戳字符串
        
        TimeStamp() : deviceUs(0), systemUs(0), globalUs(0) {}
        
        // 从帧中提取时间戳
        static TimeStamp extract(std::shared_ptr<ob::Frame> frame);
        
        // 获取文件名用的时间戳
        std::string forFileName() const;
    };

    // 简化：帧元数据
    struct FrameMeta {
        OBFrameType type;           // 帧类型
        OBFormat format;            // 数据格式  
        uint64_t index;             // 帧索引
        std::string typeName;       // 帧类型名称
        std::string formatName;     // 格式名称
        TimeStamp timestamp;        // 时间戳信息
        
        // 从帧中提取元数据
        static FrameMeta extract(std::shared_ptr<ob::Frame> frame);
        
        // 生成基础文件名
        std::string baseFileName() const;
    };

    // 简化：保存信息
    struct SaveInfo {
        std::string basePath;       // 基础路径
        FrameMeta meta;             // 帧元数据
        
        SaveInfo() = default;
        SaveInfo(const std::string& path, std::shared_ptr<ob::Frame> frame);
        
        // 生成完整文件路径
        std::string filePath(const std::string& suffix = "", const std::string& ext = ".png") const;
        
        // 检查是否有效
        bool valid() const { return !basePath.empty(); }
    };

    static DumpHelper& getInstance();

    // 初始化保存路径
    bool initializeSavePath();

    // 统一的帧处理接口 - 根据配置自动处理所有相关操作
    void processFrame(std::shared_ptr<ob::Frame> frame);

    // 保存帧数据 - 简化的统一接口
    void save(std::shared_ptr<ob::Frame> frame, const std::string& path);
    
    // 保存元数据到文件（使用 MetadataHelper 组件）
    void saveMetadata(std::shared_ptr<ob::Frame> frame, const std::string& path);
    
    // 显示元数据到控制台（使用 MetadataHelper 组件）
    void displayMetadata(std::shared_ptr<ob::Frame> frame, int interval);

private:
    DumpHelper();
    ~DumpHelper() = default;
    DumpHelper(const DumpHelper&) = delete;
    DumpHelper& operator=(const DumpHelper&) = delete;
    
    // 通用保存方法
    bool saveImage(const cv::Mat& image, const SaveInfo& info, 
                   const std::string& suffix = "", const std::string& ext = ".png");
                   
    bool saveText(const std::string& content, const SaveInfo& info,
                  const std::string& suffix = "", const std::string& ext = ".txt");
    
    // 具体保存方法 - 简化版本
    void saveColor(std::shared_ptr<ob::Frame> frame, const SaveInfo& info);
    void saveDepth(std::shared_ptr<ob::Frame> frame, const SaveInfo& info);
    void saveIR(std::shared_ptr<ob::Frame> frame, const SaveInfo& info);
    void saveIMU(std::shared_ptr<ob::Frame> frame, const SaveInfo& info);
    void savePoints(std::shared_ptr<ob::Frame> frame, const SaveInfo& info);
    
    // 深度处理
    void saveDepthColormap(std::shared_ptr<ob::DepthFrame> depth, const SaveInfo& info);
    void saveDepthData(std::shared_ptr<ob::DepthFrame> depth, const SaveInfo& info);
    cv::Mat createColormap(std::shared_ptr<ob::DepthFrame> depth);
    
    // 简化的工具方法
    bool shouldSave(OBFrameType frameType) const;
    cv::Mat convertVideoFrame(std::shared_ptr<ob::VideoFrame> frame);
    
    // SDK格式转换器
    std::shared_ptr<ob::FormatConvertFilter> formatFilter_;
    
    // 元数据处理组件
    MetadataHelper* metadataHelper_;
}; 