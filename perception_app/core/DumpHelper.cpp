#include "DumpHelper.hpp"
#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <filesystem>
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
    // 生成统一的时间戳
    auto timestamp = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(timestamp);
    std::stringstream timeStr;
    timeStr << std::put_time(std::localtime(&time), "%Y%m%d_%H%M%S");
    std::string timeStamp = timeStr.str();
    
    // 添加毫秒部分
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
        // 使用ConfigHelper确保目录存在
        std::string normPath = ConfigHelper::ensureDirectoryExists(path);
        if(normPath.empty()) {
            LOG_ERROR("无法创建或访问目录: ", path);
            return;
        }
        
        // 生成统一时间戳
        auto timeStamp = std::to_string(frame->timeStamp());
        
        // 根据帧类型保存
        if(isVideoFrame(frame->type())) {
            if(frame->type() == OB_FRAME_DEPTH) {
                saveDepthFrame(frame->as<ob::DepthFrame>(), normPath, timeStamp);
            }
            else if(frame->type() == OB_FRAME_COLOR) {
                saveColorFrame(frame->as<ob::ColorFrame>(), normPath, timeStamp);
            }
        }
        else if(isIMUFrame(frame->type())) {
            saveIMUFrame(frame, normPath, timeStamp);
        }
    }
    catch(const std::exception& e) {
        LOG_ERROR("保存帧时发生错误: ", e.what());
    }
}

bool DumpHelper::isVideoFrame(OBFrameType type) {
    return type == OB_FRAME_COLOR || 
           type == OB_FRAME_DEPTH || 
           type == OB_FRAME_IR || 
           type == OB_FRAME_IR_LEFT || 
           type == OB_FRAME_IR_RIGHT;
}

bool DumpHelper::isIMUFrame(OBFrameType type) {
    return type == OB_FRAME_ACCEL || type == OB_FRAME_GYRO;
}

void DumpHelper::saveDepthFrame(const std::shared_ptr<ob::DepthFrame> depthFrame, 
                               const std::string& path, const std::string& timeStamp) {
    if(!depthFrame) return;

    // 使用统一的时间戳和帧类型命名
    std::stringstream ss;
    ss << path << timeStamp << "_Depth.png";
    std::string filePath = ss.str();

    try {
        std::vector<int> params;
        params.push_back(cv::IMWRITE_PNG_COMPRESSION);
        params.push_back(0);
        params.push_back(cv::IMWRITE_PNG_STRATEGY);
        params.push_back(cv::IMWRITE_PNG_STRATEGY_DEFAULT);

        cv::Mat depthMat(depthFrame->height(), depthFrame->width(), CV_16UC1, depthFrame->data());
        if(cv::imwrite(filePath, depthMat, params)) {
            LOG_DEBUG("深度图保存成功: ", filePath);
        } else {
            LOG_ERROR("深度图保存失败: ", filePath);
        }
    }
    catch(const std::exception& e) {
        LOG_ERROR("保存深度帧时发生错误: ", e.what());
    }
}

void DumpHelper::saveColorFrame(std::shared_ptr<ob::ColorFrame> colorFrame, 
                               const std::string& path, const std::string& timeStamp) {
    if(!colorFrame) return;

    try {
        // 转换颜色格式为RGB
        if(colorFrame->format() != OB_FORMAT_RGB) {
            std::shared_ptr<ob::ColorFrame> convertedFrame;
            if(colorFrame->format() == OB_FORMAT_MJPG) {
                formatConverter_->setFormatConvertType(FORMAT_MJPG_TO_RGB);
            }
            else if(colorFrame->format() == OB_FORMAT_UYVY) {
                formatConverter_->setFormatConvertType(FORMAT_UYVY_TO_RGB);
            }
            else if(colorFrame->format() == OB_FORMAT_YUYV) {
                formatConverter_->setFormatConvertType(FORMAT_YUYV_TO_RGB);
            }
            else {
                LOG_ERROR("不支持的彩色格式: ", colorFrame->format());
                return;
            }
            convertedFrame = formatConverter_->process(colorFrame)->as<ob::ColorFrame>();
            colorFrame = convertedFrame;
        }

        // 转换为BGR格式用于OpenCV保存
        formatConverter_->setFormatConvertType(FORMAT_RGB_TO_BGR);
        auto bgrFrame = formatConverter_->process(colorFrame)->as<ob::ColorFrame>();

        // 使用统一的时间戳和帧类型命名
        std::stringstream ss;
        ss << path << timeStamp << "_Color.png";
        std::string filePath = ss.str();

        std::vector<int> params;
        params.push_back(cv::IMWRITE_PNG_COMPRESSION);
        params.push_back(0);
        params.push_back(cv::IMWRITE_PNG_STRATEGY);
        params.push_back(cv::IMWRITE_PNG_STRATEGY_DEFAULT);

        cv::Mat colorMat(bgrFrame->height(), bgrFrame->width(), CV_8UC3, bgrFrame->data());
        if(cv::imwrite(filePath, colorMat, params)) {
            LOG_DEBUG("彩色图保存成功: ", filePath);
        } else {
            LOG_ERROR("彩色图保存失败: ", filePath);
        }
    }
    catch(const std::exception& e) {
        LOG_ERROR("保存彩色帧时发生错误: ", e.what());
    }
}

void DumpHelper::saveIMUFrame(const std::shared_ptr<ob::Frame> frame, 
                             const std::string& path, const std::string& timeStamp) {
    if(!frame) return;

    // 使用统一的时间戳和帧类型命名
    std::stringstream ss;
    ss << path << timeStamp << "_IMU.txt";
    std::string filePath = ss.str();
    
    try {
        std::ofstream file(filePath);
        if(file.is_open()) {
            file << "Frame Type: " << (frame->type() == OB_FRAME_ACCEL ? "Accelerometer" : "Gyroscope") << "\n";
            file << "Frame Index: " << frame->index() << "\n";
            file << "Timestamp: " << frame->timeStamp() << " ms\n";
            file << "Data Size: " << frame->dataSize() << " bytes\n";
            file.close();
            LOG_DEBUG("IMU数据保存成功: ", filePath);
        } else {
            LOG_ERROR("无法创建IMU数据文件: ", filePath);
        }
    }
    catch(const std::exception& e) {
        LOG_ERROR("保存IMU帧时发生错误: ", e.what());
    }
} 