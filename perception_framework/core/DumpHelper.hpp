#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <functional>
#include <opencv2/opencv.hpp>
#include "libobsensor/ObSensor.hpp"

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

    // 简化：格式转换器
    class Converter {
    public:
        virtual ~Converter() = default;
        virtual bool supports(OBFormat format) const = 0;
        virtual cv::Mat convert(std::shared_ptr<ob::VideoFrame> frame) const = 0;
        virtual std::string fileExt() const { return ".png"; }
        virtual std::vector<int> saveParams() const;
    };

    static DumpHelper& getInstance();

    // 初始化保存路径
    bool initializeSavePath();

    // 保存帧数据
    void save(std::shared_ptr<ob::Frame> frame, const std::string& path);

private:
    DumpHelper();
    ~DumpHelper() = default;
    DumpHelper(const DumpHelper&) = delete;
    DumpHelper& operator=(const DumpHelper&) = delete;

    // 简化：帧保存函数类型
    using SaveHandler = std::function<void(std::shared_ptr<ob::Frame>, const SaveInfo&)>;
    std::unordered_map<OBFrameType, SaveHandler> handlers_;
    
    // 简化：格式转换器映射
    std::unordered_map<OBFormat, std::unique_ptr<Converter>> converters_;
    
    // 初始化处理器
    void initHandlers();
    void initConverters();
    
    // 通用保存方法
    bool saveImage(const cv::Mat& image, const SaveInfo& info, 
                   const std::string& suffix = "", const std::string& ext = ".png",
                   const std::vector<int>& params = {});
                   
    bool saveText(const std::string& content, const SaveInfo& info,
                  const std::string& suffix = "", const std::string& ext = ".txt");
    
    // 具体保存方法
    void saveColor(std::shared_ptr<ob::Frame> frame, const SaveInfo& info);
    void saveDepth(std::shared_ptr<ob::Frame> frame, const SaveInfo& info);
    void saveIR(std::shared_ptr<ob::Frame> frame, const SaveInfo& info);
    void saveIMU(std::shared_ptr<ob::Frame> frame, const SaveInfo& info);
    void savePoints(std::shared_ptr<ob::Frame> frame, const SaveInfo& info);
    
    // 深度处理
    void saveDepthColormap(std::shared_ptr<ob::DepthFrame> depth, const SaveInfo& info);
    void saveDepthData(std::shared_ptr<ob::DepthFrame> depth, const SaveInfo& info);
    cv::Mat createColormap(std::shared_ptr<ob::DepthFrame> depth);
    
    // 工具方法
    cv::Mat processVideo(std::shared_ptr<ob::VideoFrame> video);
    bool needsConversion(OBFormat format) const;
    bool shouldSave(OBFrameType frameType) const;
    std::vector<int> defaultParams() const;
    
    // SDK格式转换器
    std::shared_ptr<ob::FormatConvertFilter> formatFilter_;
};

// 具体转换器实现 - 使用更简单的命名
class RGBConverter : public DumpHelper::Converter {
public:
    bool supports(OBFormat format) const override;
    cv::Mat convert(std::shared_ptr<ob::VideoFrame> frame) const override;
};

class YUVConverter : public DumpHelper::Converter {
public:
    bool supports(OBFormat format) const override;
    cv::Mat convert(std::shared_ptr<ob::VideoFrame> frame) const override;
};

class JPEGConverter : public DumpHelper::Converter {
public:
    bool supports(OBFormat format) const override;
    cv::Mat convert(std::shared_ptr<ob::VideoFrame> frame) const override;
};

class DepthConverter : public DumpHelper::Converter {
public:
    bool supports(OBFormat format) const override;
    cv::Mat convert(std::shared_ptr<ob::VideoFrame> frame) const override;
};

class IRConverter : public DumpHelper::Converter {
public:
    bool supports(OBFormat format) const override;
    cv::Mat convert(std::shared_ptr<ob::VideoFrame> frame) const override;
}; 