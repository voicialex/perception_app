#include "DumpHelper.hpp"
#include "MetadataHelper.hpp"
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
    try {
        LOG_DEBUG("formatName called with format: ", static_cast<int>(format));
        
        switch (format) {
            case OB_FORMAT_RGB: return "RGB";
            case OB_FORMAT_BGR: return "BGR";
            case OB_FORMAT_YUYV: return "YUYV";
            case OB_FORMAT_UYVY: return "UYVY";
            case OB_FORMAT_MJPG: return "MJPG";
            case OB_FORMAT_Y8: return "Y8";
            case OB_FORMAT_Y16: return "Y16";
            case OB_FORMAT_Z16: return "Z16";
            case OB_FORMAT_RGBA: return "RGBA";
            case OB_FORMAT_BGRA: return "BGRA";
            case OB_FORMAT_YUY2: return "YUY2";
            case OB_FORMAT_NV12: return "NV12";
            case OB_FORMAT_NV21: return "NV21";
            case OB_FORMAT_H264: return "H264";
            case OB_FORMAT_H265: return "H265";
            case OB_FORMAT_HEVC: return "HEVC";
            case OB_FORMAT_I420: return "I420";
            case OB_FORMAT_GRAY: return "GRAY";
            case OB_FORMAT_Y10: return "Y10";
            case OB_FORMAT_Y11: return "Y11";
            case OB_FORMAT_Y12: return "Y12";
            case OB_FORMAT_Y14: return "Y14";
            case OB_FORMAT_ACCEL: return "ACCEL";
            case OB_FORMAT_GYRO: return "GYRO";
            case OB_FORMAT_POINT: return "POINT";
            case OB_FORMAT_RGB_POINT: return "RGB_POINT";
            case OB_FORMAT_RLE: return "RLE";
            case OB_FORMAT_RVL: return "RVL";
            case OB_FORMAT_COMPRESSED: return "COMPRESSED";
            case OB_FORMAT_YV12: return "YV12";
            case OB_FORMAT_BA81: return "BA81";
            case OB_FORMAT_BYR2: return "BYR2";
            case OB_FORMAT_RW16: return "RW16";
            case OB_FORMAT_UNKNOWN: return "UNKNOWN";
            default: 
                LOG_WARN("Unknown format: ", static_cast<int>(format));
                return "UNKNOWN_" + std::to_string(static_cast<int>(format));
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Exception in formatName with format ", static_cast<int>(format), ": ", e.what());
        return "ERROR_" + std::to_string(static_cast<int>(format));
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
       << "-" << std::to_string(index)
       << "-" << processedTypeName;
    
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

// ==================== DumpHelper Main Implementation ====================

DumpHelper& DumpHelper::getInstance() {
    static DumpHelper inst;
    return inst;
}

DumpHelper::DumpHelper() {
    formatFilter_ = std::make_shared<ob::FormatConvertFilter>();
    metadataHelper_ = &MetadataHelper::getInstance();
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

void DumpHelper::processFrame(std::shared_ptr<ob::Frame> frame) {
    if (!frame) return;
    
    auto& config = ConfigHelper::getInstance();
    
    // 使用统一的帧间隔检查
    bool shouldProcess = (frame->getIndex() % config.saveConfig.frameInterval == 0);
    
    if (!shouldProcess) return;
    
    try {
        // Process metadata for console display (if enabled)
        if (config.saveConfig.enableMetadataConsole) {
            displayMetadata(frame, config.saveConfig.frameInterval);
        }

        // Process frame saving (if enabled)
        if (config.saveConfig.enableDump) {
            // Save frame data
            std::string frameTypeStr = frameTypeName(frame->getType());
            LOG_DEBUG("Saving frame, type: ", frameTypeStr, ", index: ", frame->getIndex());
            save(frame, config.saveConfig.dumpPath);
            
            // Save metadata file (if enabled)
            if (config.saveConfig.saveMetadata) {
                LOG_DEBUG("Saving metadata for frame, type: ", frameTypeStr, 
                          ", index: ", frame->getIndex());
                saveMetadata(frame, config.saveConfig.dumpPath);
            }
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error processing frame in DumpHelper: ", e.what());
    }
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
        
        // 使用简化的switch分发
        switch (info.meta.type) {
            case OB_FRAME_COLOR:
                saveColor(frame, info);
                break;
            case OB_FRAME_DEPTH:
                saveDepth(frame, info);
                break;
            case OB_FRAME_IR:
            case OB_FRAME_IR_LEFT:
            case OB_FRAME_IR_RIGHT:
                saveIR(frame, info);
                break;
            case OB_FRAME_ACCEL:
            case OB_FRAME_GYRO:
                saveIMU(frame, info);
                break;
            case OB_FRAME_POINTS:
                savePoints(frame, info);
                break;
            default:
                LOG_WARN("Unsupported frame type: ", info.meta.typeName);
                break;
        }
        
        LOG_DEBUG("Saved frame type: ", info.meta.typeName);
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error saving frame: ", e.what());
    }
}

bool DumpHelper::saveImage(const cv::Mat& image, const SaveInfo& info, 
                           const std::string& suffix, const std::string& ext) {
    if (image.empty()) {
        LOG_ERROR("Invalid image for ", info.meta.typeName, ": image is empty");
        return false;
    }
    
    if (!info.valid()) {
        LOG_ERROR("Invalid save info for ", info.meta.typeName);
        return false;
    }
    
    // 验证图像数据
    if (!image.data) {
        LOG_ERROR("Invalid image for ", info.meta.typeName, ": null data pointer");
        return false;
    }
    
    if (image.rows <= 0 || image.cols <= 0) {
        LOG_ERROR("Invalid image for ", info.meta.typeName, ": invalid dimensions ", 
                  image.cols, "x", image.rows);
        return false;
    }
    
    try {
        std::string filePath = info.filePath(suffix, ext);
        
        // 检查文件扩展名，确保使用正确的保存参数
        std::vector<int> params;
        if (ext == ".png") {
            params = {
                cv::IMWRITE_PNG_COMPRESSION, 1,  // 降低压缩级别，提高兼容性
                cv::IMWRITE_PNG_STRATEGY, cv::IMWRITE_PNG_STRATEGY_DEFAULT
            };
        } else if (ext == ".jpg" || ext == ".jpeg") {
            params = {
                cv::IMWRITE_JPEG_QUALITY, 95
            };
        }
        // 其他格式使用默认参数
        
        LOG_DEBUG("Attempting to save ", info.meta.typeName, 
                  " image: ", filePath, 
                  ", size: ", image.cols, "x", image.rows,
                  ", type: ", image.type(),
                  ", channels: ", image.channels());
        
        bool success = false;
        if (params.empty()) {
            success = cv::imwrite(filePath, image);
        } else {
            success = cv::imwrite(filePath, image, params);
        }
        
        if (success) {
            LOG_DEBUG(info.meta.typeName, 
                     (suffix.empty() ? "" : " " + suffix), 
                     " saved: ", filePath);
            return true;
        } else {
            LOG_ERROR("Failed to save ", info.meta.typeName, ": ", filePath,
                      " (cv::imwrite returned false)");
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

cv::Mat DumpHelper::convertVideoFrame(std::shared_ptr<ob::VideoFrame> video) {
    if (!video) {
        LOG_ERROR("convertVideoFrame: null video frame");
        return cv::Mat();
    }
    
    OBFormat format = video->getFormat();
    int width = video->getWidth();
    int height = video->getHeight();
    uint8_t* data = video->getData();
    uint32_t dataSize = video->getDataSize();
    
    // 验证基本参数
    if (width <= 0 || height <= 0) {
        LOG_ERROR("convertVideoFrame: invalid dimensions ", width, "x", height);
        return cv::Mat();
    }
    
    if (!data) {
        LOG_ERROR("convertVideoFrame: null data pointer");
        return cv::Mat();
    }
    
    if (dataSize == 0) {
        LOG_ERROR("convertVideoFrame: zero data size");
        return cv::Mat();
    }
    
    LOG_DEBUG("convertVideoFrame: format=", static_cast<int>(format), 
              ", size=", width, "x", height, 
              ", dataSize=", dataSize);
    
    try {
        switch (format) {
            case OB_FORMAT_RGB:
            case OB_FORMAT_BGR: {
                uint32_t expectedSize = width * height * 3;
                if (dataSize < expectedSize) {
                    LOG_ERROR("convertVideoFrame: insufficient data size for RGB/BGR: ", 
                              dataSize, " < ", expectedSize);
                    return cv::Mat();
                }
                cv::Mat result(height, width, CV_8UC3, data);
                return result.clone(); // 返回深拷贝以避免数据生命周期问题
            }
                
            case OB_FORMAT_RGBA:
            case OB_FORMAT_BGRA: {
                uint32_t expectedSize = width * height * 4;
                if (dataSize < expectedSize) {
                    LOG_ERROR("convertVideoFrame: insufficient data size for RGBA/BGRA: ", 
                              dataSize, " < ", expectedSize);
                    return cv::Mat();
                }
                cv::Mat result(height, width, CV_8UC4, data);
                return result.clone();
            }
                
            case OB_FORMAT_Y8: {
                uint32_t expectedSize = width * height;
                if (dataSize < expectedSize) {
                    LOG_ERROR("convertVideoFrame: insufficient data size for Y8: ", 
                              dataSize, " < ", expectedSize);
                    return cv::Mat();
                }
                cv::Mat result(height, width, CV_8UC1, data);
                return result.clone();
            }
                
            case OB_FORMAT_Y16:
            case OB_FORMAT_Z16: {
                uint32_t expectedSize = width * height * 2;
                if (dataSize < expectedSize) {
                    LOG_ERROR("convertVideoFrame: insufficient data size for Y16/Z16: ", 
                              dataSize, " < ", expectedSize);
                    return cv::Mat();
                }
                cv::Mat result(height, width, CV_16UC1, data);
                return result.clone();
            }
                
            case OB_FORMAT_YUYV: {
                uint32_t expectedSize = width * height * 2;
                if (dataSize < expectedSize) {
                    LOG_ERROR("convertVideoFrame: insufficient data size for YUYV: ", 
                              dataSize, " < ", expectedSize);
                    return cv::Mat();
                }
                cv::Mat yuv(height, width, CV_8UC2, data);
                cv::Mat bgr;
                cv::cvtColor(yuv, bgr, cv::COLOR_YUV2BGR_YUYV);
                return bgr;
            }
            
            case OB_FORMAT_UYVY: {
                uint32_t expectedSize = width * height * 2;
                if (dataSize < expectedSize) {
                    LOG_ERROR("convertVideoFrame: insufficient data size for UYVY: ", 
                              dataSize, " < ", expectedSize);
                    return cv::Mat();
                }
                cv::Mat yuv(height, width, CV_8UC2, data);
                cv::Mat bgr;
                cv::cvtColor(yuv, bgr, cv::COLOR_YUV2BGR_UYVY);
                return bgr;
            }
            
            case OB_FORMAT_MJPG: {
                if (dataSize < 10) { // JPEG最小长度检查
                    LOG_ERROR("convertVideoFrame: insufficient data size for MJPG: ", dataSize);
                    return cv::Mat();
                }
                std::vector<uint8_t> jpeg(data, data + dataSize);
                cv::Mat result = cv::imdecode(jpeg, cv::IMREAD_COLOR);
                if (result.empty()) {
                    LOG_ERROR("convertVideoFrame: failed to decode MJPG data");
                }
                return result;
            }
            
            default:
                LOG_WARN("convertVideoFrame: unsupported video format: ", static_cast<int>(format));
                // 基本处理作为后备
                int matType = (format == OB_FORMAT_Y16) ? CV_16UC1 : CV_8UC1;
                uint32_t expectedSize = width * height * ((matType == CV_16UC1) ? 2 : 1);
                if (dataSize < expectedSize) {
                    LOG_ERROR("convertVideoFrame: insufficient data size for fallback: ", 
                              dataSize, " < ", expectedSize);
                    return cv::Mat();
                }
                cv::Mat result(height, width, matType, data);
                return result.clone();
        }
    } catch (const std::exception& e) {
        LOG_ERROR("convertVideoFrame: exception during conversion: ", e.what());
        return cv::Mat();
    }
}

void DumpHelper::saveColor(std::shared_ptr<ob::Frame> frame, const SaveInfo& info) {
    try {
        auto colorFrame = frame->as<ob::ColorFrame>();
        if (!colorFrame) {
            LOG_ERROR("Failed to convert to ColorFrame");
            return;
        }
        
        int width = colorFrame->getWidth();
        int height = colorFrame->getHeight();
        uint8_t* data = colorFrame->getData();
        uint32_t dataSize = colorFrame->getDataSize();
        OBFormat format = colorFrame->getFormat();
        
        // 验证基本数据
        if (width <= 0 || height <= 0) {
            LOG_ERROR("saveColor: invalid dimensions ", width, "x", height);
            return;
        }
        
        if (!data) {
            LOG_ERROR("saveColor: null data pointer");
            return;
        }
        
        if (dataSize == 0) {
            LOG_ERROR("saveColor: zero data size");
            return;
        }
        
        LOG_DEBUG("saveColor: format=", static_cast<int>(format), 
                  ", size=", width, "x", height, 
                  ", dataSize=", dataSize);
        
        // 如果是RGB/BGR格式，直接使用
        if (format == OB_FORMAT_RGB || format == OB_FORMAT_BGR) {
            uint32_t expectedSize = width * height * 3;
            if (dataSize < expectedSize) {
                LOG_ERROR("saveColor: insufficient data size for RGB/BGR: ", 
                          dataSize, " < ", expectedSize);
                return;
            }
            
            cv::Mat colorMat(height, width, CV_8UC3, data);
            cv::Mat colorCopy = colorMat.clone();
            
            if (colorCopy.empty() || !colorCopy.data) {
                LOG_ERROR("saveColor: failed to create color matrix");
                return;
            }
            
            saveImage(colorCopy, info);
            return;
        }
        
        // 对于其他格式，需要转换
        std::shared_ptr<ob::ColorFrame> processed = colorFrame;
        
        // 使用SDK进行格式转换
        try {
            if (format == OB_FORMAT_MJPG) {
                formatFilter_->setFormatConvertType(FORMAT_MJPG_TO_RGB);
                processed = formatFilter_->process(colorFrame)->as<ob::ColorFrame>();
            } else if (format == OB_FORMAT_UYVY) {
                formatFilter_->setFormatConvertType(FORMAT_UYVY_TO_RGB);
                processed = formatFilter_->process(colorFrame)->as<ob::ColorFrame>();
            } else if (format == OB_FORMAT_YUYV) {
                formatFilter_->setFormatConvertType(FORMAT_YUYV_TO_RGB);
                processed = formatFilter_->process(colorFrame)->as<ob::ColorFrame>();
            } else {
                // 尝试使用convertVideoFrame作为后备
                cv::Mat colorMat = convertVideoFrame(colorFrame);
                if (!colorMat.empty()) {
                    saveImage(colorMat, info);
                    return;
                }
                LOG_ERROR("Unsupported color format: ", static_cast<int>(format));
                return;
            }
            
            // 确保转换为BGR格式
            if (processed && processed->getFormat() != OB_FORMAT_BGR) {
                formatFilter_->setFormatConvertType(FORMAT_RGB_TO_BGR);
                processed = formatFilter_->process(processed)->as<ob::ColorFrame>();
            }
            
            if (!processed) {
                LOG_ERROR("saveColor: format conversion failed");
                return;
            }
            
            // 验证转换后的数据
            int procWidth = processed->getWidth();
            int procHeight = processed->getHeight();
            uint8_t* procData = processed->getData();
            uint32_t procDataSize = processed->getDataSize();
            
            if (procWidth <= 0 || procHeight <= 0 || !procData || procDataSize == 0) {
                LOG_ERROR("saveColor: invalid processed frame data");
                return;
            }
            
            cv::Mat colorMat(procHeight, procWidth, CV_8UC3, procData);
            cv::Mat colorCopy = colorMat.clone();
            
            if (colorCopy.empty() || !colorCopy.data) {
                LOG_ERROR("saveColor: failed to create processed color matrix");
                return;
            }
            
            saveImage(colorCopy, info);
            
        } catch (const ob::Error& e) {
            LOG_ERROR("saveColor: SDK format conversion error: ", e.getMessage());
            
            // 尝试后备方案
            cv::Mat colorMat = convertVideoFrame(colorFrame);
            if (!colorMat.empty()) {
                saveImage(colorMat, info);
            } else {
                LOG_ERROR("saveColor: all conversion methods failed");
            }
        }
        
    } catch (const std::exception& e) {
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
        
        int width = depthFrame->getWidth();
        int height = depthFrame->getHeight();
        uint8_t* data = depthFrame->getData();
        uint32_t dataSize = depthFrame->getDataSize();
        
        // 验证深度帧数据
        if (width <= 0 || height <= 0) {
            LOG_ERROR("saveDepth: invalid dimensions ", width, "x", height);
            return;
        }
        
        if (!data) {
            LOG_ERROR("saveDepth: null data pointer");
                return;
            }
        
        uint32_t expectedSize = width * height * 2; // 16位深度数据
        if (dataSize < expectedSize) {
            LOG_ERROR("saveDepth: insufficient data size: ", dataSize, " < ", expectedSize);
            return;
        }
        
        // 保存原始深度 - 使用深拷贝
        cv::Mat depthMat(height, width, CV_16UC1, data);
        cv::Mat depthCopy = depthMat.clone();
        
        if (depthCopy.empty() || !depthCopy.data) {
            LOG_ERROR("saveDepth: failed to create depth matrix");
            return;
        }
        
        saveImage(depthCopy, info);

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
        
        int width = videoFrame->getWidth();
        int height = videoFrame->getHeight();
        uint8_t* data = videoFrame->getData();
        uint32_t dataSize = videoFrame->getDataSize();
        OBFormat format = frame->getFormat();
        
        // 验证基本数据
        if (width <= 0 || height <= 0) {
            LOG_ERROR("saveIR: invalid dimensions ", width, "x", height);
            return;
        }
        
        if (!data) {
            LOG_ERROR("saveIR: null data pointer");
            return;
        }
        
        if (dataSize == 0) {
            LOG_ERROR("saveIR: zero data size");
            return;
        }
        
        int matType;
        uint32_t expectedSize;
        
        switch (format) {
            case OB_FORMAT_Y8:
                matType = CV_8UC1;
                expectedSize = width * height;
                break;
            case OB_FORMAT_Y16:
                matType = CV_16UC1;
                expectedSize = width * height * 2;
                break;
            default:
                LOG_ERROR("Unsupported IR format: ", static_cast<int>(format));
                return;
        }
        
        if (dataSize < expectedSize) {
            LOG_ERROR("saveIR: insufficient data size: ", dataSize, " < ", expectedSize);
            return;
        }
        
        cv::Mat irMat(height, width, matType, data);
        cv::Mat irCopy = irMat.clone();
        
        if (irCopy.empty() || !irCopy.data) {
            LOG_ERROR("saveIR: failed to create IR matrix");
            return;
        }
        
        saveImage(irCopy, info);
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

void DumpHelper::saveMetadata(std::shared_ptr<ob::Frame> frame, const std::string& path) {
    if (!frame || !metadataHelper_) return;
    
    try {
        SaveInfo info(path, frame);
        if (!info.valid()) {
            return;
        }
        
        // 使用 MetadataHelper 组件提取元数据内容
        std::string metadataContent = metadataHelper_->extractMetadataToString(frame);
        
        // 保存到文件
        saveText(metadataContent, info, "metadata", ".txt");
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error saving metadata: ", e.what());
    }
}

void DumpHelper::displayMetadata(std::shared_ptr<ob::Frame> frame, int interval) {
    if (!frame || !metadataHelper_) return;
    
    try {
        // 使用 MetadataHelper 组件进行控制台显示
        metadataHelper_->printMetadata(frame, interval);
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error displaying metadata: ", e.what());
    }
} 