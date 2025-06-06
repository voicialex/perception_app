#include "DumpHelper.hpp"
#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include "ConfigHelper.hpp"
#include "Logger.hpp"

DumpHelper& DumpHelper::getInstance() {
    static DumpHelper instance;
    return instance;
}

DumpHelper::DumpHelper() {
    formatConverter_ = std::make_shared<ob::FormatConvertFilter>();
}

bool DumpHelper::initializeSavePath() {
    auto& config = ConfigHelper::getInstance();
    
    // 只有在启用数据保存时才检查路径
    if (!config.saveConfig.enableDump) {
        LOG_DEBUG("Data saving disabled, skipping path initialization");
        return true;
    }
    
    // 使用Logger的ensureDirectoryExists来创建目录
    std::string normalizedPath = Logger::ensureDirectoryExists(config.saveConfig.dumpPath, true);
    
    if (normalizedPath.empty()) {
        LOG_ERROR("Failed to create or access dump directory: ", config.saveConfig.dumpPath);
        LOG_ERROR("Disabling data dump to prevent further errors");
        config.saveConfig.enableDump = false;
        return false;
    }
    
    // 更新配置中的路径为规范化后的路径
    config.saveConfig.dumpPath = normalizedPath;
    
    LOG_INFO("Data save path initialized: ", normalizedPath);
    return true;
}

std::string DumpHelper::generateTimeStamp() {
    // Generate unified timestamp
    auto timestamp = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(timestamp);
    std::stringstream timeStr;
    timeStr << std::put_time(std::localtime(&time), "%Y%m%d_%H%M%S");
    std::string timeStamp = timeStr.str();
    
    // Add millisecond part
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        timestamp.time_since_epoch()) % 1000;
    std::stringstream msStr;
    msStr << std::setfill('0') << std::setw(3) << ms.count();
    timeStamp += "_" + msStr.str();
    
    return timeStamp;
}

void DumpHelper::saveFrame(std::shared_ptr<ob::Frame> frame, const std::string& path) {
    if(!frame) return;

    try {
        // Use Logger to ensure directory exists
        std::string normPath = Logger::ensureDirectoryExists(path);
        if(normPath.empty()) {
            LOG_ERROR("Failed to create or access directory: ", path);
            return;
        }
        
        // Get config to check which frame types to save
        auto& config = ConfigHelper::getInstance();
        
        // Save based on frame type
        bool frameSaved = false;
        
        if(frame->type() == OB_FRAME_COLOR && config.saveConfig.saveColor && config.streamConfig.enableColor) {
            saveColorFrame(frame->as<ob::ColorFrame>(), normPath);
            frameSaved = true;
        }
        else if(frame->type() == OB_FRAME_DEPTH && config.saveConfig.saveDepth && config.streamConfig.enableDepth) {
            saveDepthFrame(frame->as<ob::DepthFrame>(), normPath);
            frameSaved = true;
        }
        else if(is_ir_frame(frame->type()) && config.saveConfig.saveIR && 
                (config.streamConfig.enableIR || config.streamConfig.enableIRLeft || config.streamConfig.enableIRRight)) {
            saveIRFrame(frame, normPath);
            frameSaved = true;
        }
        else if((frame->type() == OB_FRAME_ACCEL || frame->type() == OB_FRAME_GYRO) && config.streamConfig.enableIMU) {
            saveIMUFrame(frame, normPath);
            frameSaved = true;
        }
        
        if(!frameSaved) {
            std::string frameTypeName = ob::TypeHelper::convertOBFrameTypeToString(frame->type());
            LOG_DEBUG("Frame type ", frameTypeName, " (", static_cast<int>(frame->type()), ") not saved - stream disabled or save option disabled");
        }
    }
    catch(const std::exception& e) {
        LOG_ERROR("Error saving frame: ", e.what());
    }
}

std::vector<int> DumpHelper::createPNGParams() {
    std::vector<int> params;
    params.push_back(cv::IMWRITE_PNG_COMPRESSION);
    params.push_back(0);
    params.push_back(cv::IMWRITE_PNG_STRATEGY);
    params.push_back(cv::IMWRITE_PNG_STRATEGY_DEFAULT);
    return params;
}

std::string DumpHelper::createFilePath(const std::string& path, 
                                     const std::string& timeStamp, 
                                     const std::string& frameTypeName,
                                     const std::string& extension) {
    // 处理帧类型名称，将空格替换为下划线
    std::string processedTypeName = frameTypeName;
    std::replace(processedTypeName.begin(), processedTypeName.end(), ' ', '_');
    
    std::stringstream ss;
    ss << path << timeStamp << "_" << processedTypeName << extension;
    return ss.str();
}

bool DumpHelper::saveImageFrame(const std::shared_ptr<ob::VideoFrame> videoFrame,
                              const std::string& path,
                              const std::string& timeStamp,
                              const std::string& frameTypeName,
                              int cvMatType) {
    if (!videoFrame) {
        LOG_ERROR("Invalid video frame");
        return false;
    }
    
    try {
        // 创建OpenCV Mat
        cv::Mat imageMat(videoFrame->height(), videoFrame->width(), cvMatType, videoFrame->data());
        
        // 调用Mat版本的saveImageFrame来实际保存图像
        return saveImageFrame(imageMat, path, timeStamp, frameTypeName);
    }
    catch (const std::exception& e) {
        LOG_ERROR("Error saving image frame: ", e.what());
        return false;
    }
}

bool DumpHelper::saveImageFrame(const cv::Mat& imageMat,
                               const std::string& path,
                               const std::string& timeStamp,
                               const std::string& frameTypeName,
                               const std::string& extension) {
    if (imageMat.empty()) {
        LOG_ERROR("Invalid image matrix");
        return false;
    }
    
    try {
        // 创建文件路径
        std::string filePath = createFilePath(path, timeStamp, frameTypeName, extension);
        
        // 获取保存参数
        auto params = createPNGParams();
        
        // 保存图像
        if (cv::imwrite(filePath, imageMat, params)) {
            LOG_DEBUG(frameTypeName, " image saved successfully: ", filePath);
            return true;
        } else {
            LOG_ERROR("Failed to save ", frameTypeName, " image: ", filePath);
            return false;
        }
    }
    catch (const std::exception& e) {
        LOG_ERROR("Error saving image frame: ", e.what());
        return false;
    }
}

cv::Mat DumpHelper::createDepthColormap(const std::shared_ptr<ob::DepthFrame> depthFrame) {
    if (!depthFrame) {
        return cv::Mat();
    }
    
    try {
        // 参考utils_opencv.cpp中的深度图像处理逻辑
        cv::Mat rawMat = cv::Mat(depthFrame->height(), depthFrame->width(), CV_16UC1, depthFrame->data());
        
        // depth frame pixel value multiply scale to get distance in millimeter
        float scale = depthFrame->getValueScale();

        cv::Mat cvtMat;
        // normalization to 0-255. 0.032f is 256/8000, to limit the range of depth to 8000mm
        rawMat.convertTo(cvtMat, CV_32F, scale * 0.032f);

        // apply gamma correction to enhance the contrast for near objects
        cv::pow(cvtMat, 0.6f, cvtMat);

        //  convert to 8-bit
        cvtMat.convertTo(cvtMat, CV_8UC1, 10);  // multiplier 10 is to normalize to 0-255 (nearly) after applying gamma correction

        // apply colormap
        cv::Mat colormapMat;
        cv::applyColorMap(cvtMat, colormapMat, cv::COLORMAP_JET);
        
        return colormapMat;
    }
    catch (const std::exception& e) {
        LOG_ERROR("Error creating depth colormap: ", e.what());
        return cv::Mat();
    }
}

void DumpHelper::saveDepthColormapFrame(const std::shared_ptr<ob::DepthFrame> depthFrame,
                                        const std::string& path) {
    if (!depthFrame) return;
    
    try {
        // 生成时间戳
        std::string timeStamp = std::to_string(depthFrame->timeStamp());
        
        // 获取帧类型名称
        std::string frameTypeName = ob::TypeHelper::convertOBFrameTypeToString(depthFrame->type());
        
        // 创建深度colormap
        cv::Mat colormapMat = createDepthColormap(depthFrame);
        if (!colormapMat.empty()) {
            // 使用通用图像保存方法保存colormap
            saveImageFrame(colormapMat, path, timeStamp, frameTypeName + "_colormap");
        }
    }
    catch (const std::exception& e) {
        LOG_ERROR("Error saving depth colormap: ", e.what());
    }
}

void DumpHelper::saveDepthFrame(const std::shared_ptr<ob::DepthFrame> depthFrame, 
                               const std::string& path) {
    if (!depthFrame) return;
    
    // 生成时间戳
    std::string timeStamp = std::to_string(depthFrame->timeStamp());
    
    // 获取帧类型名称
    std::string frameTypeName = ob::TypeHelper::convertOBFrameTypeToString(depthFrame->type());
    
    // 保存原始深度图像
    saveImageFrame(depthFrame, path, timeStamp, frameTypeName, CV_16UC1);

    // 如果启用了colormap保存，则额外保存一份colormap版本
    auto& config = ConfigHelper::getInstance();

    if (config.saveConfig.saveDepthColormap) {
        saveDepthColormapFrame(depthFrame, path);
    }
}

void DumpHelper::saveColorFrame(std::shared_ptr<ob::ColorFrame> colorFrame, 
                               const std::string& path) {
    if (!colorFrame) return;
    
    // 生成时间戳
    std::string timeStamp = std::to_string(colorFrame->timeStamp());
    
    // 获取帧类型名称
    std::string frameTypeName = ob::TypeHelper::convertOBFrameTypeToString(colorFrame->type());
    
    try {
        // 将格式转换为RGB
        if (colorFrame->format() != OB_FORMAT_RGB) {
            std::shared_ptr<ob::ColorFrame> convertedFrame;
            if (colorFrame->format() == OB_FORMAT_MJPG) {
                formatConverter_->setFormatConvertType(FORMAT_MJPG_TO_RGB);
            }
            else if (colorFrame->format() == OB_FORMAT_UYVY) {
                formatConverter_->setFormatConvertType(FORMAT_UYVY_TO_RGB);
            }
            else if (colorFrame->format() == OB_FORMAT_YUYV) {
                formatConverter_->setFormatConvertType(FORMAT_YUYV_TO_RGB);
            }
            else {
                LOG_ERROR("Unsupported color format: ", colorFrame->format());
                return;
            }
            convertedFrame = formatConverter_->process(colorFrame)->as<ob::ColorFrame>();
            colorFrame = convertedFrame;
        }
        
        // 转换为BGR格式以便OpenCV保存
        formatConverter_->setFormatConvertType(FORMAT_RGB_TO_BGR);
        auto bgrFrame = formatConverter_->process(colorFrame)->as<ob::ColorFrame>();
        
        // 使用通用方法保存
        saveImageFrame(bgrFrame, path, timeStamp, frameTypeName, CV_8UC3);
    }
    catch (const std::exception& e) {
        LOG_ERROR("Error saving color frame: ", e.what());
    }
}

void DumpHelper::saveIRFrame(std::shared_ptr<ob::Frame> irFrame, 
                            const std::string& path) {
    if (!irFrame) return;
    
    // 生成时间戳
    std::string timeStamp = std::to_string(irFrame->timeStamp());
    
    // 获取IR类型名称
    std::string irTypeName = ob::TypeHelper::convertOBFrameTypeToString(irFrame->type());
    
    try {
        // 将Frame转换为VideoFrame
        auto videoFrame = irFrame->as<ob::VideoFrame>();
        if (!videoFrame) {
            LOG_ERROR("Failed to convert IR frame to VideoFrame");
            return;
        }
        
        // 根据格式选择正确的Mat类型
        int matType;
        switch (irFrame->format()) {
            case OB_FORMAT_Y8:
                matType = CV_8UC1;
                break;
            case OB_FORMAT_Y16:
                matType = CV_16UC1;
                break;
            default:
                LOG_ERROR("Unsupported IR format: ", irFrame->format());
                return;
        }
        
        // 使用通用方法保存
        saveImageFrame(videoFrame, path, timeStamp, irTypeName, matType);
    }
    catch (const std::exception& e) {
        LOG_ERROR("Error saving IR frame: ", e.what());
    }
}

void DumpHelper::saveIMUFrame(const std::shared_ptr<ob::Frame> frame, 
                             const std::string& path) {
    if (!frame) return;
    
    LOG_DEBUG("Saving IMU frame, index: ", frame->index());
    
    // 生成时间戳
    std::string timeStamp = std::to_string(frame->timeStamp());
    
    // 获取帧类型名称
    std::string frameTypeName = ob::TypeHelper::convertOBFrameTypeToString(frame->type());
    
    try {
        // 创建文件路径（使用.txt扩展名）
        std::string filePath = createFilePath(path, timeStamp, frameTypeName, ".txt");
        
        std::ofstream file(filePath);
        if (file.is_open()) {
            file << "Frame Type: " << frameTypeName << "\n";
            file << "Frame Index: " << frame->index() << "\n";
            file << "Timestamp: " << frame->timeStamp() << " ms\n";
            file << "Data Size: " << frame->dataSize() << " bytes\n";
            file.close();
            LOG_DEBUG(frameTypeName, " data saved successfully: ", filePath);
        } else {
            LOG_ERROR("Failed to create ", frameTypeName, " data file: ", filePath);
        }
    }
    catch (const std::exception& e) {
        LOG_ERROR("Error saving IMU frame: ", e.what());
    }
} 