#include "CalibrationManager.hpp"
#include "CVWindow.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>

namespace calibration {

CalibrationManager& CalibrationManager::getInstance() {
    static CalibrationManager instance;
    return instance;
}

CalibrationManager::~CalibrationManager() {
    stop();
}

bool CalibrationManager::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (initialized_) {
        LOG_WARN("CalibrationManager already initialized");
        return true;
    }
    
    // 初始化成员变量
    state_ = CalibrationState::IDLE;
    imagePoints_.clear();
    objectPoints_.clear();
    imageSize_ = cv::Size(0, 0);
    lastResult_ = CalibrationResult{};
    lastCaptureTime_ = std::chrono::steady_clock::now();
    
    initialized_ = true;
    LOG_INFO("CalibrationManager initialized successfully");
    return true;
}

bool CalibrationManager::startCalibration(const ConfigHelper::CalibrationConfig& config, 
                                         CalibrationProgressCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        LOG_ERROR("CalibrationManager not initialized");
        return false;
    }
    
    if (state_ != CalibrationState::IDLE) {
        LOG_WARN("Calibration already in progress");
        return false;
    }
    
    if (!config.validate()) {
        LOG_ERROR("Invalid calibration configuration");
        return false;
    }
    
    config_ = config;
    progressCallback_ = callback;
    
    // 清除之前的数据
    imagePoints_.clear();
    objectPoints_.clear();
    imageSize_ = cv::Size(0, 0);
    lastResult_ = CalibrationResult{};
    
    // 创建保存目录
    try {
        if (!config_.saveDirectory.empty()) {
            std::filesystem::create_directories(config_.saveDirectory);
        }
    } catch (const std::exception& e) {
        LOG_WARN("Failed to create calibration directory: ", e.what());
    }
    
    state_ = CalibrationState::COLLECTING;
    lastCaptureTime_ = std::chrono::steady_clock::now();
    
    LOG_INFO("Camera calibration started");
    LOG_INFO("  Board size: ", config_.boardWidth, "x", config_.boardHeight);
    LOG_INFO("  Square size: ", config_.squareSize, " mm");
    LOG_INFO("  Min frames: ", config_.minValidFrames);
    LOG_INFO("  Max frames: ", config_.maxFrames);
    
    if (progressCallback_) {
        progressCallback_(state_, 0, config_.maxFrames, "标定开始");
    }
    
    return true;
}

void CalibrationManager::stopCalibration() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (state_ == CalibrationState::IDLE) {
        return;
    }
    
    state_ = CalibrationState::IDLE;
    progressCallback_ = nullptr;
    
    LOG_INFO("Camera calibration stopped");
}

bool CalibrationManager::processFrame(const cv::Mat& frame) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_ || state_ != CalibrationState::COLLECTING || frame.empty()) {
        return false;
    }
    
    // 检查是否应该采集当前帧
    if (!shouldCaptureFrame()) {
        return false;
    }
    
    // 检查是否已采集足够的帧
    if (static_cast<int>(imagePoints_.size()) >= config_.maxFrames) {
        LOG_INFO("Reached maximum number of frames, starting calibration");
        state_ = CalibrationState::PROCESSING;
        
        // 在另一个线程中执行标定计算
        std::thread([this]() {
            CalibrationResult result;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                result = performCalibration();
                lastResult_ = result;
            }
            
            std::lock_guard<std::mutex> lock(mutex_);
            if (result.isValid) {
                state_ = CalibrationState::COMPLETED;
                LOG_INFO("Calibration completed successfully");
                
                // 保存结果
                if (!config_.saveDirectory.empty()) {
                    saveCalibrationResult(result);
                }
                
                if (progressCallback_) {
                    progressCallback_(state_, static_cast<int>(imagePoints_.size()), 
                                    config_.maxFrames, "标定完成");
                }
            } else {
                state_ = CalibrationState::FAILED;
                LOG_ERROR("Calibration failed");
                
                if (progressCallback_) {
                    progressCallback_(state_, static_cast<int>(imagePoints_.size()), 
                                    config_.maxFrames, "标定失败");
                }
            }
        }).detach();
        
        return true;
    }
    
    // 转换为灰度图像
    cv::Mat gray;
    if (frame.channels() == 3) {
        cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = frame;
    }
    
    // 查找棋盘角点
    std::vector<cv::Point2f> corners;
    cv::Size boardSize(config_.boardWidth, config_.boardHeight);
    
    bool found = cv::findChessboardCorners(gray, boardSize, corners, 
                                          cv::CALIB_CB_ADAPTIVE_THRESH | 
                                          cv::CALIB_CB_NORMALIZE_IMAGE);
    
    if (found) {
        // 亚像素精度角点检测
        if (config_.useSubPixel) {
            cv::cornerSubPix(gray, corners, cv::Size(11, 11), cv::Size(-1, -1),
                           cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::COUNT, 
                                          30, 0.1));
        }
        
        // 添加角点
        imagePoints_.push_back(corners);
        
        // 生成对应的世界坐标点
        objectPoints_.push_back(generateChessboardPoints());
        
        // 记录图像大小
        if (imageSize_.width == 0) {
            imageSize_ = gray.size();
        }
        
        // 更新最后采集时间
        lastCaptureTime_ = std::chrono::steady_clock::now();
        
        int currentFrames = static_cast<int>(imagePoints_.size());
        LOG_INFO("Captured calibration frame ", currentFrames, "/", config_.maxFrames);
        
        if (progressCallback_) {
            std::string message = "已采集 " + std::to_string(currentFrames) + " 帧";
            progressCallback_(state_, currentFrames, config_.maxFrames, message);
        }
        
        // 检查是否达到最小帧数，可以进行标定
        if (currentFrames >= config_.minValidFrames) {
            LOG_INFO("Minimum frames collected, can perform calibration");
        }
        
        return true;
    }
    
    return false;
}

bool CalibrationManager::processFrame(std::shared_ptr<ob::Frame> frame) {
    if (!frame) {
        return false;
    }
    
    cv::Mat image = convertFrameToMat(frame);
    if (image.empty()) {
        return false;
    }
    
    return processFrame(image);
}

CalibrationResult CalibrationManager::performCalibration() {
    CalibrationResult result;
    
    if (imagePoints_.empty() || objectPoints_.empty()) {
        LOG_ERROR("No calibration data available");
        return result;
    }
    
    if (imagePoints_.size() < static_cast<size_t>(config_.minValidFrames)) {
        LOG_ERROR("Insufficient calibration frames: ", imagePoints_.size(), 
                  " < ", config_.minValidFrames);
        return result;
    }
    
    try {
        LOG_INFO("Starting calibration computation with ", imagePoints_.size(), " frames");
        
        // 执行相机标定
        cv::Mat cameraMatrix, distCoeffs;
        std::vector<cv::Mat> rvecs, tvecs;
        
        double rms = cv::calibrateCamera(objectPoints_, imagePoints_, imageSize_,
                                       cameraMatrix, distCoeffs, rvecs, tvecs);
        
        // 填充结果
        result.cameraMatrix = cameraMatrix;
        result.distCoeffs = distCoeffs;
        result.rvecs = rvecs;
        result.tvecs = tvecs;
        result.rms = rms;
        result.imageSize = imageSize_;
        result.isValid = true;
        
        LOG_INFO("Calibration completed successfully");
        LOG_INFO("  RMS reprojection error: ", rms);
        LOG_INFO("  Camera matrix: \n", cameraMatrix);
        LOG_INFO("  Distortion coefficients: \n", distCoeffs);
        
    } catch (const cv::Exception& e) {
        LOG_ERROR("OpenCV calibration error: ", e.what());
        result.isValid = false;
    } catch (const std::exception& e) {
        LOG_ERROR("Calibration error: ", e.what());
        result.isValid = false;
    }
    
    return result;
}

bool CalibrationManager::saveCalibrationResult(const CalibrationResult& result, 
                                              const std::string& filename) {
    if (!result.isValid) {
        LOG_ERROR("Cannot save invalid calibration result");
        return false;
    }
    
    try {
        std::string filepath = config_.saveDirectory + filename + ".xml";
        cv::FileStorage fs(filepath, cv::FileStorage::WRITE);
        
        if (!fs.isOpened()) {
            LOG_ERROR("Failed to open file for writing: ", filepath);
            return false;
        }
        
        // 保存标定结果
        fs << "camera_matrix" << result.cameraMatrix;
        fs << "distortion_coefficients" << result.distCoeffs;
        fs << "image_width" << result.imageSize.width;
        fs << "image_height" << result.imageSize.height;
        fs << "rms_reprojection_error" << result.rms;
        fs << "calibration_time" << cv::format("%04d-%02d-%02d %02d:%02d:%02d", 
                                               2024, 1, 1, 12, 0, 0); // 简化时间戳
        
        fs.release();
        
        LOG_INFO("Calibration result saved to: ", filepath);
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error saving calibration result: ", e.what());
        return false;
    }
}

CalibrationResult CalibrationManager::loadCalibrationResult(const std::string& filename) {
    CalibrationResult result;
    
    try {
        std::string filepath = config_.saveDirectory + filename + ".xml";
        cv::FileStorage fs(filepath, cv::FileStorage::READ);
        
        if (!fs.isOpened()) {
            LOG_ERROR("Failed to open calibration file: ", filepath);
            return result;
        }
        
        // 加载标定结果
        fs["camera_matrix"] >> result.cameraMatrix;
        fs["distortion_coefficients"] >> result.distCoeffs;
        fs["image_width"] >> result.imageSize.width;
        fs["image_height"] >> result.imageSize.height;
        fs["rms_reprojection_error"] >> result.rms;
        
        fs.release();
        
        // 检查加载的数据是否有效
        if (!result.cameraMatrix.empty() && !result.distCoeffs.empty() &&
            result.imageSize.width > 0 && result.imageSize.height > 0) {
            result.isValid = true;
            LOG_INFO("Calibration result loaded from: ", filepath);
        } else {
            LOG_ERROR("Invalid calibration data in file: ", filepath);
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error loading calibration result: ", e.what());
    }
    
    return result;
}

bool CalibrationManager::undistortImage(const cv::Mat& src, cv::Mat& dst, 
                                       const CalibrationResult& result) {
    if (!result.isValid || src.empty()) {
        return false;
    }
    
    try {
        cv::undistort(src, dst, result.cameraMatrix, result.distCoeffs);
        return true;
    } catch (const cv::Exception& e) {
        LOG_ERROR("Error in undistortion: ", e.what());
        return false;
    }
}

cv::Mat CalibrationManager::drawChessboardCorners(const cv::Mat& image, 
                                                 const std::vector<cv::Point2f>& corners, 
                                                 bool found) {
    cv::Mat result = image.clone();
    
    if (found && !corners.empty()) {
        cv::Size boardSize(9, 6); // 默认棋盘大小
        cv::drawChessboardCorners(result, boardSize, corners, found);
    }
    
    return result;
}

void CalibrationManager::setProgressCallback(CalibrationProgressCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    progressCallback_ = callback;
}

void CalibrationManager::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        return;
    }
    
    // 停止标定
    if (state_ != CalibrationState::IDLE) {
        state_ = CalibrationState::IDLE;
    }
    
    progressCallback_ = nullptr;
    initialized_ = false;
    
    LOG_INFO("CalibrationManager stopped");
}

cv::Mat CalibrationManager::convertFrameToMat(std::shared_ptr<ob::Frame> frame) {
    if (!frame) {
        return cv::Mat();
    }
    
    try {
        // 使用与examples中相同的实现方式
        auto vf = frame->as<ob::VideoFrame>();
        const uint32_t height = vf->getHeight();
        const uint32_t width = vf->getWidth();

        if (frame->getFormat() == OB_FORMAT_BGR) {
            return cv::Mat(height, width, CV_8UC3, frame->getData());
        }
        else if(frame->getFormat() == OB_FORMAT_RGB) {
            // The color channel for RGB images in opencv is BGR, so we need to convert it to BGR before returning the cv::Mat
            auto rgbMat = cv::Mat(height, width, CV_8UC3, frame->getData());
            cv::Mat bgrMat;
            cv::cvtColor(rgbMat, bgrMat, cv::COLOR_RGB2BGR);
            return bgrMat;
        }
        else if(frame->getFormat() == OB_FORMAT_Y16) {
            return cv::Mat(height, width, CV_16UC1, frame->getData());
        }
        
        LOG_WARN("Unsupported frame format for calibration: ", static_cast<int>(frame->getFormat()));
        return cv::Mat();
    } catch (const std::exception& e) {
        LOG_ERROR("Error converting frame to Mat: ", e.what());
        return cv::Mat();
    }
}

std::vector<cv::Point3f> CalibrationManager::generateChessboardPoints() {
    std::vector<cv::Point3f> corners;
    
    for (int i = 0; i < config_.boardHeight; ++i) {
        for (int j = 0; j < config_.boardWidth; ++j) {
            corners.push_back(cv::Point3f(j * config_.squareSize, 
                                        i * config_.squareSize, 0));
        }
    }
    
    return corners;
}

bool CalibrationManager::shouldCaptureFrame() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastCaptureTime_).count();
    
    return elapsed >= (config_.minInterval * 1000); // 转换为毫秒
}

} // namespace calibration 