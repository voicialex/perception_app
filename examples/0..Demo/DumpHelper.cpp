#include "DumpHelper.hpp"
#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>

DumpHelper& DumpHelper::getInstance() {
    static DumpHelper instance;
    return instance;
}

DumpHelper::DumpHelper() {
    formatConverter_ = std::make_shared<ob::FormatConvertFilter>();
}

void DumpHelper::saveFrame(std::shared_ptr<ob::Frame> frame, const std::string& path) {
    if(!frame) return;

    if(isVideoFrame(frame->type())) {
        if(frame->type() == OB_FRAME_DEPTH) {
            saveDepthFrame(frame->as<ob::DepthFrame>(), path);
        }
        else if(frame->type() == OB_FRAME_COLOR) {
            saveColorFrame(frame->as<ob::ColorFrame>(), path);
        }
    }
    else if(isIMUFrame(frame->type())) {
        saveIMUFrame(frame, path);
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

void DumpHelper::saveDepthFrame(const std::shared_ptr<ob::DepthFrame> depthFrame, const std::string& path) {
    if(!depthFrame) return;

    auto timestamp = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(timestamp);
    std::stringstream ss;
    ss << path << "/Depth_" << std::put_time(std::localtime(&time), "%Y%m%d_%H%M%S") 
       << "_" << depthFrame->index() << "_" << depthFrame->timeStamp() << "ms.png";

    std::vector<int> params;
    params.push_back(cv::IMWRITE_PNG_COMPRESSION);
    params.push_back(0);
    params.push_back(cv::IMWRITE_PNG_STRATEGY);
    params.push_back(cv::IMWRITE_PNG_STRATEGY_DEFAULT);

    cv::Mat depthMat(depthFrame->height(), depthFrame->width(), CV_16UC1, depthFrame->data());
    cv::imwrite(ss.str(), depthMat, params);
    std::cout << "Depth saved: " << ss.str() << std::endl;
}

void DumpHelper::saveColorFrame(std::shared_ptr<ob::ColorFrame> colorFrame, const std::string& path) {
    if(!colorFrame) return;

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
            std::cout << "Color format is not supported!" << std::endl;
            return;
        }
        convertedFrame = formatConverter_->process(colorFrame)->as<ob::ColorFrame>();
        colorFrame = convertedFrame;
    }

    // 转换为BGR格式用于OpenCV保存
    formatConverter_->setFormatConvertType(FORMAT_RGB_TO_BGR);
    auto bgrFrame = formatConverter_->process(colorFrame)->as<ob::ColorFrame>();

    auto timestamp = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(timestamp);
    std::stringstream ss;
    ss << path << "/Color_" << std::put_time(std::localtime(&time), "%Y%m%d_%H%M%S") 
       << "_" << bgrFrame->index() << "_" << bgrFrame->timeStamp() << "ms.png";

    std::vector<int> params;
    params.push_back(cv::IMWRITE_PNG_COMPRESSION);
    params.push_back(0);
    params.push_back(cv::IMWRITE_PNG_STRATEGY);
    params.push_back(cv::IMWRITE_PNG_STRATEGY_DEFAULT);

    cv::Mat colorMat(bgrFrame->height(), bgrFrame->width(), CV_8UC3, bgrFrame->data());
    cv::imwrite(ss.str(), colorMat, params);
    std::cout << "Color saved: " << ss.str() << std::endl;
}

void DumpHelper::saveIMUFrame(const std::shared_ptr<ob::Frame> frame, const std::string& path) {
    if(!frame) return;

    auto timestamp = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(timestamp);
    std::stringstream ss;
    ss << path << "/IMU_" << std::put_time(std::localtime(&time), "%Y%m%d_%H%M%S") 
       << "_" << frame->index() << "_" << frame->timeStamp() << "ms.txt";
    
    std::ofstream file(ss.str());
    if(file.is_open()) {
        file << "Frame Type: " << (frame->type() == OB_FRAME_ACCEL ? "Accelerometer" : "Gyroscope") << "\n";
        file << "Frame Index: " << frame->index() << "\n";
        file << "Timestamp: " << frame->timeStamp() << " ms\n";
        file << "Data Size: " << frame->dataSize() << " bytes\n";
        file.close();
        std::cout << "IMU data saved: " << ss.str() << std::endl;
    }
} 