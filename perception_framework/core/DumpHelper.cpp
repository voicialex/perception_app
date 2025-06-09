#include "DumpHelper.hpp"
#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include "ConfigHelper.hpp"
#include "Logger.hpp"

// 简化的格式转换函数
std::string formatName(OBFormat format) {
    switch (format) {
        case OB_FORMAT_RGB: return "RGB";
        case OB_FORMAT_BGR: return "BGR";
        case OB_FORMAT_YUYV: return "YUYV";
        case OB_FORMAT_UYVY: return "UYVY";
        case OB_FORMAT_MJPG: return "MJPG";
        case OB_FORMAT_Y8: return "Y8";
        case OB_FORMAT_Y16: return "Y16";
        default: return "";
    }
}

// 简化的帧类型转换函数
std::string frameTypeName(OBFrameType frameType) {
    switch (frameType) {
        case OB_FRAME_COLOR: return "Color";
        case OB_FRAME_DEPTH: return "Depth";
        case OB_FRAME_IR: return "IR";
        case OB_FRAME_IR_LEFT: return "IR_Left";
        case OB_FRAME_IR_RIGHT: return "IR_Right";
        case OB_FRAME_ACCEL: return "Accel";
        case OB_FRAME_GYRO: return "Gyro";
        case OB_FRAME_POINTS: return "Points";
        default: return "Unknown";
    }
}

// ==================== TimeStamp Implementation ====================

DumpHelper::TimeStamp DumpHelper::TimeStamp::extract(std::shared_ptr<ob::Frame> frame) {
    TimeStamp ts;
    
    if (!frame) return ts;
    
    try {
        // 获取各种时间戳
        ts.deviceUs = frame->getTimeStampUs();
        ts.systemUs = frame->getSystemTimeStampUs();
        ts.globalUs = frame->getGlobalTimeStampUs();
        
        // 生成设备时间戳字符串
        auto timestampMs = ts.deviceUs / 1000;
        auto time = std::chrono::system_clock::from_time_t(timestampMs / 1000);
        auto timeT = std::chrono::system_clock::to_time_t(time);
        
        std::stringstream ss;
        ss << std::put_time(std::localtime(&timeT), "%Y%m%d_%H%M%S");
        ss << "_" << std::setfill('0') << std::setw(3) << (timestampMs % 1000);
        ts.deviceStr = ss.str();
        
        // 生成系统时间戳字符串
        auto sysTimeMs = ts.systemUs / 1000;
        auto sysTime = std::chrono::system_clock::from_time_t(sysTimeMs / 1000);
        auto sysTimeT = std::chrono::system_clock::to_time_t(sysTime);
        
        std::stringstream sysS;
        sysS << std::put_time(std::localtime(&sysTimeT), "%Y%m%d_%H%M%S");
        sysS << "_" << std::setfill('0') << std::setw(3) << (sysTimeMs % 1000);
        ts.systemStr = sysS.str();
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error extracting timestamp: ", e.what());
    }
    
    return ts;
}

std::string DumpHelper::TimeStamp::forFileName() const {
    return !systemStr.empty() ? systemStr : deviceStr;
}

// ==================== FrameMeta Implementation ====================

DumpHelper::FrameMeta DumpHelper::FrameMeta::extract(std::shared_ptr<ob::Frame> frame) {
    FrameMeta meta;
    
    if (!frame) return meta;
    
    try {
        meta.type = frame->getType();
        meta.format = frame->getFormat();
        meta.index = frame->getIndex();
        meta.typeName = frameTypeName(meta.type);
        meta.formatName = ::formatName(meta.format);
        meta.timestamp = TimeStamp::extract(frame);
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error extracting frame meta: ", e.what());
    }
    
    return meta;
}

std::string DumpHelper::FrameMeta::baseFileName() const {
    std::string processedTypeName = typeName;
    std::replace(processedTypeName.begin(), processedTypeName.end(), ' ', '_');
    
    std::stringstream ss;
    ss << timestamp.forFileName() 
       << "_" << std::to_string(index)
       << "_" << processedTypeName;
    
    if (!formatName.empty()) {
        ss << "_" << formatName;
    }
    
    return ss.str();
}

// ==================== SaveInfo Implementation ====================

DumpHelper::SaveInfo::SaveInfo(const std::string& path, std::shared_ptr<ob::Frame> frame) {
    basePath = Logger::ensureDirectoryExists(path);
    meta = FrameMeta::extract(frame);
}

std::string DumpHelper::SaveInfo::filePath(const std::string& suffix, const std::string& ext) const {
    std::stringstream ss;
    ss << basePath << meta.baseFileName();
    
    if (!suffix.empty()) {
        ss << "_" << suffix;
    }
    
    ss << ext;
    return ss.str();
}

// ==================== Converter Base Implementation ====================

std::vector<int> DumpHelper::Converter::saveParams() const {
    return {
        cv::IMWRITE_PNG_COMPRESSION, 0,
        cv::IMWRITE_PNG_STRATEGY, cv::IMWRITE_PNG_STRATEGY_DEFAULT
    };
}

// ==================== DumpHelper Main Implementation ====================

DumpHelper& DumpHelper::getInstance() {
    static DumpHelper inst;
    return inst;
}

DumpHelper::DumpHelper() {
    formatFilter_ = std::make_shared<ob::FormatConvertFilter>();
    initHandlers();
    initConverters();
}

void DumpHelper::initHandlers() {
    handlers_[OB_FRAME_COLOR] = [this](std::shared_ptr<ob::Frame> frame, const SaveInfo& info) {
        saveColor(frame, info);
    };
    
    handlers_[OB_FRAME_DEPTH] = [this](std::shared_ptr<ob::Frame> frame, const SaveInfo& info) {
        saveDepth(frame, info);
    };
    
    handlers_[OB_FRAME_IR] = [this](std::shared_ptr<ob::Frame> frame, const SaveInfo& info) {
        saveIR(frame, info);
    };
    
    handlers_[OB_FRAME_IR_LEFT] = [this](std::shared_ptr<ob::Frame> frame, const SaveInfo& info) {
        saveIR(frame, info);
    };
    
    handlers_[OB_FRAME_IR_RIGHT] = [this](std::shared_ptr<ob::Frame> frame, const SaveInfo& info) {
        saveIR(frame, info);
    };
    
    handlers_[OB_FRAME_ACCEL] = [this](std::shared_ptr<ob::Frame> frame, const SaveInfo& info) {
        saveIMU(frame, info);
    };
    
    handlers_[OB_FRAME_GYRO] = [this](std::shared_ptr<ob::Frame> frame, const SaveInfo& info) {
        saveIMU(frame, info);
    };
    
    handlers_[OB_FRAME_POINTS] = [this](std::shared_ptr<ob::Frame> frame, const SaveInfo& info) {
        savePoints(frame, info);
    };
}

void DumpHelper::initConverters() {
    converters_[OB_FORMAT_RGB] = std::make_unique<RGBConverter>();
    converters_[OB_FORMAT_BGR] = std::make_unique<RGBConverter>();
    converters_[OB_FORMAT_YUYV] = std::make_unique<YUVConverter>();
    converters_[OB_FORMAT_UYVY] = std::make_unique<YUVConverter>();
    converters_[OB_FORMAT_MJPG] = std::make_unique<JPEGConverter>();
    converters_[OB_FORMAT_Y8] = std::make_unique<IRConverter>();
    converters_[OB_FORMAT_Y16] = std::make_unique<DepthConverter>();
}

bool DumpHelper::initializeSavePath() {
    auto& config = ConfigHelper::getInstance();
    
    if (!config.saveConfig.enableDump) {
        LOG_DEBUG("Data saving disabled");
        return true;
    }
    
    std::string normalizedPath = Logger::ensureDirectoryExists(config.saveConfig.dumpPath, true);
    
    if (normalizedPath.empty()) {
        LOG_ERROR("Failed to create dump directory: ", config.saveConfig.dumpPath);
        config.saveConfig.enableDump = false;
        return false;
    }
    
    config.saveConfig.dumpPath = normalizedPath;
    LOG_INFO("Data save path initialized: ", normalizedPath);
    return true;
}

void DumpHelper::save(std::shared_ptr<ob::Frame> frame, const std::string& path) {
    if (!frame) return;

    try {
        SaveInfo info(path, frame);
        if (!info.valid()) {
            return;
        }
        
        // 检查是否应该保存此类型的帧
        if (!shouldSave(info.meta.type)) {
            LOG_DEBUG("Frame type ", info.meta.typeName, " not saved - disabled");
            return;
        }
        
        // 使用处理器分发
        auto it = handlers_.find(info.meta.type);
        if (it != handlers_.end()) {
            it->second(frame, info);
            LOG_DEBUG("Saved frame type: ", info.meta.typeName);
        } else {
            LOG_WARN("No handler for frame type: ", info.meta.typeName);
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error saving frame: ", e.what());
    }
}

std::vector<int> DumpHelper::defaultParams() const {
    return {
        cv::IMWRITE_PNG_COMPRESSION, 0,
        cv::IMWRITE_PNG_STRATEGY, cv::IMWRITE_PNG_STRATEGY_DEFAULT
    };
}

bool DumpHelper::saveImage(const cv::Mat& image, const SaveInfo& info, 
                           const std::string& suffix, const std::string& ext,
                           const std::vector<int>& params) {
    if (image.empty()) {
        LOG_ERROR("Invalid image for ", info.meta.typeName);
        return false;
    }
    
    if (!info.valid()) {
        LOG_ERROR("Invalid save info for ", info.meta.typeName);
        return false;
    }
    
    try {
        std::string filePath = info.filePath(suffix, ext);
        auto saveParams = params.empty() ? defaultParams() : params;
        
        if (cv::imwrite(filePath, image, saveParams)) {
            LOG_DEBUG(info.meta.typeName, 
                     (suffix.empty() ? "" : " " + suffix), 
                     " saved: ", filePath);
            return true;
        } else {
            LOG_ERROR("Failed to save ", info.meta.typeName, ": ", filePath);
            return false;
        }
    }
    catch (const std::exception& e) {
        LOG_ERROR("Error saving ", info.meta.typeName, ": ", e.what());
        return false;
    }
}

bool DumpHelper::saveText(const std::string& content, const SaveInfo& info,
                          const std::string& suffix, const std::string& ext) {
    if (!info.valid()) {
        LOG_ERROR("Invalid save info for ", info.meta.typeName);
        return false;
    }
    
    try {
        std::string filePath = info.filePath(suffix, ext);
        std::ofstream file(filePath);
        
        if (file.is_open()) {
            file << content;
            file.close();
            LOG_DEBUG(info.meta.typeName, 
                     (suffix.empty() ? "" : " " + suffix), 
                     " saved: ", filePath);
            return true;
        } else {
            LOG_ERROR("Failed to create ", info.meta.typeName, " file: ", filePath);
            return false;
        }
    }
    catch (const std::exception& e) {
        LOG_ERROR("Error saving ", info.meta.typeName, " text: ", e.what());
        return false;
    }
}

cv::Mat DumpHelper::processVideo(std::shared_ptr<ob::VideoFrame> video) {
    if (!video) return cv::Mat();
    
    OBFormat format = video->getFormat();
    
    // 查找转换器
    auto it = converters_.find(format);
    if (it != converters_.end()) {
        return it->second->convert(video);
    }
    
    LOG_WARN("No converter for format ", static_cast<int>(format));
    
    // 基本处理
    int matType = (format == OB_FORMAT_Y16) ? CV_16UC1 : CV_8UC1;
    return cv::Mat(video->getHeight(), video->getWidth(), matType, video->getData());
}

void DumpHelper::saveColor(std::shared_ptr<ob::Frame> frame, const SaveInfo& info) {
    try {
        auto colorFrame = frame->as<ob::ColorFrame>();
        if (!colorFrame) {
            LOG_ERROR("Failed to convert to ColorFrame");
            return;
        }
        
        std::shared_ptr<ob::ColorFrame> processed = colorFrame;
        
        // 格式转换
        if (needsConversion(colorFrame->getFormat())) {
            if (colorFrame->getFormat() == OB_FORMAT_MJPG) {
                formatFilter_->setFormatConvertType(FORMAT_MJPG_TO_RGB);
            }
            else if (colorFrame->getFormat() == OB_FORMAT_UYVY) {
                formatFilter_->setFormatConvertType(FORMAT_UYVY_TO_RGB);
            }
            else if (colorFrame->getFormat() == OB_FORMAT_YUYV) {
                formatFilter_->setFormatConvertType(FORMAT_YUYV_TO_RGB);
            }
            else {
                LOG_ERROR("Unsupported color format: ", static_cast<int>(colorFrame->getFormat()));
                return;
            }
            processed = formatFilter_->process(colorFrame)->as<ob::ColorFrame>();
        }
        
        // 转换为BGR
        if (processed->getFormat() != OB_FORMAT_BGR) {
            formatFilter_->setFormatConvertType(FORMAT_RGB_TO_BGR);
            processed = formatFilter_->process(processed)->as<ob::ColorFrame>();
        }
        
        cv::Mat colorMat(processed->getHeight(), processed->getWidth(), 
                        CV_8UC3, processed->getData());
        saveImage(colorMat, info);
    }
    catch (const std::exception& e) {
        LOG_ERROR("Error saving color frame: ", e.what());
    }
}

void DumpHelper::saveDepth(std::shared_ptr<ob::Frame> frame, const SaveInfo& info) {
    try {
        auto depthFrame = frame->as<ob::DepthFrame>();
        if (!depthFrame) {
            LOG_ERROR("Failed to convert to DepthFrame");
            return;
        }
        
        // 保存原始深度
        cv::Mat depthMat(depthFrame->getHeight(), depthFrame->getWidth(), 
                        CV_16UC1, depthFrame->getData());
        saveImage(depthMat, info);

        auto& config = ConfigHelper::getInstance();

        // 保存colormap
        if (config.saveConfig.saveDepthColormap) {
            saveDepthColormap(depthFrame, info);
        }

        // 保存数据
        if (config.saveConfig.saveDepthData) {
            saveDepthData(depthFrame, info);
        }
    }
    catch (const std::exception& e) {
        LOG_ERROR("Error saving depth frame: ", e.what());
    }
}

void DumpHelper::saveIR(std::shared_ptr<ob::Frame> frame, const SaveInfo& info) {
    try {
        auto videoFrame = frame->as<ob::VideoFrame>();
        if (!videoFrame) {
            LOG_ERROR("Failed to convert IR to VideoFrame");
            return;
        }
        
        int matType;
        switch (frame->getFormat()) {
            case OB_FORMAT_Y8:
                matType = CV_8UC1;
                break;
            case OB_FORMAT_Y16:
                matType = CV_16UC1;
                break;
            default:
                LOG_ERROR("Unsupported IR format: ", static_cast<int>(frame->getFormat()));
                return;
        }
        
        cv::Mat irMat(videoFrame->getHeight(), videoFrame->getWidth(), matType, videoFrame->getData());
        saveImage(irMat, info);
    }
    catch (const std::exception& e) {
        LOG_ERROR("Error saving IR frame: ", e.what());
    }
}

void DumpHelper::saveIMU(std::shared_ptr<ob::Frame> frame, const SaveInfo& info) {
    try {
        std::stringstream content;
        content << "Frame Type: " << info.meta.typeName << "\n";
        content << "Frame Index: " << info.meta.index << "\n";
        content << "Device Timestamp: " << info.meta.timestamp.deviceUs << " us\n";
        content << "System Timestamp: " << info.meta.timestamp.systemUs << " us\n";
        content << "Data Size: " << frame->getDataSize() << " bytes\n";
        
        if (frame->getType() == OB_FRAME_ACCEL) {
            auto accelFrame = frame->as<ob::AccelFrame>();
            if (accelFrame) {
                auto value = accelFrame->getValue();
                content << "Acceleration (m/s²): X=" << value.x 
                       << ", Y=" << value.y 
                       << ", Z=" << value.z << "\n";
                content << "Temperature: " << accelFrame->getTemperature() << " °C\n";
            }
        } else if (frame->getType() == OB_FRAME_GYRO) {
            auto gyroFrame = frame->as<ob::GyroFrame>();
            if (gyroFrame) {
                auto value = gyroFrame->getValue();
                content << "Angular Velocity (rad/s): X=" << value.x 
                       << ", Y=" << value.y 
                       << ", Z=" << value.z << "\n";
                content << "Temperature: " << gyroFrame->getTemperature() << " °C\n";
            }
        }
        
        saveText(content.str(), info);
    }
    catch (const std::exception& e) {
        LOG_ERROR("Error saving IMU frame: ", e.what());
    }
}

void DumpHelper::savePoints(std::shared_ptr<ob::Frame> frame, const SaveInfo& info) {
    try {
        auto pointsFrame = frame->as<ob::PointsFrame>();
        if (!pointsFrame) {
            LOG_ERROR("Failed to convert to PointsFrame");
            return;
        }
        
        std::stringstream content;
        content << "Point Cloud Info:\n";
        content << "Format: " << static_cast<int>(pointsFrame->getFormat()) << "\n";
        content << "Width: " << pointsFrame->getWidth() << "\n";
        content << "Height: " << pointsFrame->getHeight() << "\n";
        content << "Coordinate Scale: " << pointsFrame->getCoordinateValueScale() << "\n";
        content << "Data Size: " << pointsFrame->getDataSize() << " bytes\n";
        
        saveText(content.str(), info, "info");
        
    }
    catch (const std::exception& e) {
        LOG_ERROR("Error saving points frame: ", e.what());
    }
}

void DumpHelper::saveDepthColormap(std::shared_ptr<ob::DepthFrame> depth, const SaveInfo& info) {
    cv::Mat colormap = createColormap(depth);
    if (!colormap.empty()) {
        saveImage(colormap, info, "colormap");
    }
}

void DumpHelper::saveDepthData(std::shared_ptr<ob::DepthFrame> depth, const SaveInfo& info) {
    try {
        const uint16_t* data = reinterpret_cast<const uint16_t*>(depth->getData());
        int width = depth->getWidth();
        int height = depth->getHeight();
        float scale = depth->getValueScale();
        
        std::stringstream content;
        content << "# Depth Data (mm)\n";
        content << "# Width: " << width << ", Height: " << height << "\n";
        content << "# Scale: " << scale << "\n";
        content << "Y\\X";
        for (int x = 0; x < width; x++) {
            content << "," << x;
        }
        content << "\n";
        
        for (int y = 0; y < height; y++) {
            content << y;
            for (int x = 0; x < width; x++) {
                uint16_t raw = data[y * width + x];
                float depthMm = raw * scale;
                content << "," << depthMm;
            }
            content << "\n";
        }
        
        saveText(content.str(), info, "data", ".csv");
    }
    catch (const std::exception& e) {
        LOG_ERROR("Error saving depth data: ", e.what());
    }
}

cv::Mat DumpHelper::createColormap(std::shared_ptr<ob::DepthFrame> depth) {
    if (!depth) return cv::Mat();
    
    try {
        cv::Mat raw(depth->getHeight(), depth->getWidth(), 
                   CV_16UC1, depth->getData());
        float scale = depth->getValueScale();

        cv::Mat converted;
        raw.convertTo(converted, CV_32F, scale * 0.032f);
        cv::pow(converted, 0.6f, converted);
        converted.convertTo(converted, CV_8UC1, 10);

        cv::Mat colormap;
        cv::applyColorMap(converted, colormap, cv::COLORMAP_JET);
        
        return colormap;
    }
    catch (const std::exception& e) {
        LOG_ERROR("Error creating colormap: ", e.what());
        return cv::Mat();
    }
}

bool DumpHelper::needsConversion(OBFormat format) const {
    return format != OB_FORMAT_RGB && format != OB_FORMAT_BGR;
}

bool DumpHelper::shouldSave(OBFrameType frameType) const {
    auto& config = ConfigHelper::getInstance();
    
    switch (frameType) {
        case OB_FRAME_COLOR:
            return config.saveConfig.saveColor && config.streamConfig.enableColor;
        case OB_FRAME_DEPTH:
            return config.saveConfig.saveDepth && config.streamConfig.enableDepth;
        case OB_FRAME_IR:
        case OB_FRAME_IR_LEFT:
        case OB_FRAME_IR_RIGHT:
            return config.saveConfig.saveIR && 
                   (config.streamConfig.enableIR || config.streamConfig.enableIRLeft || 
                    config.streamConfig.enableIRRight);
        case OB_FRAME_ACCEL:
        case OB_FRAME_GYRO:
            return config.streamConfig.enableIMU;
        case OB_FRAME_POINTS:
            return true;
        default:
            return false;
    }
}

// ==================== Converter Implementations ====================

// RGB Converter
bool RGBConverter::supports(OBFormat format) const {
    return format == OB_FORMAT_RGB || format == OB_FORMAT_BGR || 
           format == OB_FORMAT_RGBA || format == OB_FORMAT_BGRA;
}

cv::Mat RGBConverter::convert(std::shared_ptr<ob::VideoFrame> frame) const {
    int channels = (frame->getFormat() == OB_FORMAT_RGBA || 
                   frame->getFormat() == OB_FORMAT_BGRA) ? 4 : 3;
    int matType = (channels == 4) ? CV_8UC4 : CV_8UC3;
    
    return cv::Mat(frame->getHeight(), frame->getWidth(), matType, frame->getData());
}

// YUV Converter
bool YUVConverter::supports(OBFormat format) const {
    return format == OB_FORMAT_YUYV || format == OB_FORMAT_UYVY;
}

cv::Mat YUVConverter::convert(std::shared_ptr<ob::VideoFrame> frame) const {
    cv::Mat yuv(frame->getHeight(), frame->getWidth(), CV_8UC2, frame->getData());
    cv::Mat bgr;
    
    int conversion = (frame->getFormat() == OB_FORMAT_YUYV) ? 
                     cv::COLOR_YUV2BGR_YUYV : cv::COLOR_YUV2BGR_UYVY;
    cv::cvtColor(yuv, bgr, conversion);
    
    return bgr;
}

// JPEG Converter
bool JPEGConverter::supports(OBFormat format) const {
    return format == OB_FORMAT_MJPG;
}

cv::Mat JPEGConverter::convert(std::shared_ptr<ob::VideoFrame> frame) const {
    std::vector<uint8_t> jpeg(frame->getData(), frame->getData() + frame->getDataSize());
    return cv::imdecode(jpeg, cv::IMREAD_COLOR);
}

// Depth Converter
bool DepthConverter::supports(OBFormat format) const {
    return format == OB_FORMAT_Y16 || format == OB_FORMAT_Z16;
}

cv::Mat DepthConverter::convert(std::shared_ptr<ob::VideoFrame> frame) const {
    return cv::Mat(frame->getHeight(), frame->getWidth(), CV_16UC1, frame->getData());
}

// IR Converter
bool IRConverter::supports(OBFormat format) const {
    return format == OB_FORMAT_Y8 || format == OB_FORMAT_Y16;
}

cv::Mat IRConverter::convert(std::shared_ptr<ob::VideoFrame> frame) const {
    int matType = (frame->getFormat() == OB_FORMAT_Y16) ? CV_16UC1 : CV_8UC1;
    return cv::Mat(frame->getHeight(), frame->getWidth(), matType, frame->getData());
} 