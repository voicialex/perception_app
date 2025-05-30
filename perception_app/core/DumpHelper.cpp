#include "DumpHelper.hpp"
#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include "Logger.hpp"
#include "ConfigHelper.hpp"

DumpHelper& DumpHelper::getInstance() {
    static DumpHelper instance;
    return instance;
}

DumpHelper::DumpHelper() {
    formatConverter_ = std::make_shared<ob::FormatConvertFilter>();
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
        // Use ConfigHelper to ensure directory exists
        std::string normPath = ConfigHelper::ensureDirectoryExists(path);
        if(normPath.empty()) {
            LOG_ERROR("Failed to create or access directory: ", path);
            return;
        }
        
        // Get config to check which frame types to save
        auto& config = ConfigHelper::getInstance();
        
        // Generate unified timestamp
        auto timeStamp = std::to_string(frame->timeStamp());
        
        // 获取帧类型名称用于日志
        std::string frameTypeName = ob::TypeHelper::convertOBFrameTypeToString(frame->type());
        
        // Save based on frame type
        bool frameSaved = false;
        
        if(frame->type() == OB_FRAME_COLOR && config.saveConfig.saveColor) {
            LOG_DEBUG("Saving ", frameTypeName, " frame, index: ", frame->index());
            saveColorFrame(frame->as<ob::ColorFrame>(), normPath, timeStamp);
            frameSaved = true;
        }
        else if(frame->type() == OB_FRAME_DEPTH && config.saveConfig.saveDepth) {
            LOG_DEBUG("Saving ", frameTypeName, " frame, index: ", frame->index());
            saveDepthFrame(frame->as<ob::DepthFrame>(), normPath, timeStamp);
            frameSaved = true;
        }
        else if(is_ir_frame(frame->type()) && config.saveConfig.saveIR) {
            LOG_DEBUG("Saving ", frameTypeName, " frame, index: ", frame->index());
            saveIRFrame(frame, normPath, timeStamp);
            frameSaved = true;
        }
        else if(frame->type() == OB_FRAME_ACCEL || frame->type() == OB_FRAME_GYRO) {
            LOG_DEBUG("Saving ", frameTypeName, " frame, index: ", frame->index());
            saveIMUFrame(frame, normPath, timeStamp);
            frameSaved = true;
        }
        
        if(!frameSaved) {
            LOG_DEBUG("Frame type ", frameTypeName, " (", static_cast<int>(frame->type()), ") not saved");
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
        // 创建文件路径
        std::string filePath = createFilePath(path, timeStamp, frameTypeName);
        
        // 获取PNG保存参数
        auto params = createPNGParams();
        
        // 创建OpenCV Mat
        cv::Mat imageMat(videoFrame->height(), videoFrame->width(), cvMatType, videoFrame->data());
        
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

void DumpHelper::saveDepthFrame(const std::shared_ptr<ob::DepthFrame> depthFrame, 
                               const std::string& path, const std::string& timeStamp) {
    if (!depthFrame) return;
    
    // 获取帧类型名称
    std::string frameTypeName = ob::TypeHelper::convertOBFrameTypeToString(depthFrame->type());
    
    // 使用通用方法保存
    saveImageFrame(depthFrame, path, timeStamp, frameTypeName, CV_16UC1);
}

void DumpHelper::saveColorFrame(std::shared_ptr<ob::ColorFrame> colorFrame, 
                               const std::string& path, const std::string& timeStamp) {
    if (!colorFrame) return;
    
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
                            const std::string& path, const std::string& timeStamp) {
    if (!irFrame) return;
    
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
                             const std::string& path, const std::string& timeStamp) {
    if (!frame) return;
    
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