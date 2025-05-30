#pragma once

#include <opencv2/opencv.hpp>
#include <opencv2/calib3d.hpp>
#include <vector>
#include <string>
#include <memory>
#include <functional>
#include <mutex>
#include <atomic>
#include <thread>
#include <chrono>
#include "libobsensor/ObSensor.hpp"
#include "Logger.hpp"
#include "ConfigHelper.hpp"

namespace calibration {

/**
 * @brief 标定结果
 */
struct CalibrationResult {
    cv::Mat cameraMatrix;           // 相机内参矩阵
    cv::Mat distCoeffs;            // 畸变系数
    std::vector<cv::Mat> rvecs;    // 旋转向量
    std::vector<cv::Mat> tvecs;    // 平移向量
    double rms = 0.0;              // 重投影误差
    cv::Size imageSize;            // 图像大小
    bool isValid = false;          // 标定是否成功
    
    std::string getSummary() const {
        if (!isValid) return "标定失败";
        
        std::ostringstream oss;
        oss << "标定结果摘要:\n";
        oss << "  图像尺寸: " << imageSize.width << "x" << imageSize.height << "\n";
        oss << "  重投影误差: " << std::fixed << std::setprecision(3) << rms << " 像素\n";
        oss << "  内参矩阵:\n";
        for (int i = 0; i < 3; i++) {
            oss << "    [";
            for (int j = 0; j < 3; j++) {
                oss << std::fixed << std::setprecision(2) << cameraMatrix.at<double>(i, j);
                if (j < 2) oss << ", ";
            }
            oss << "]\n";
        }
        oss << "  畸变系数: [";
        for (int i = 0; i < distCoeffs.cols; i++) {
            oss << std::fixed << std::setprecision(6) << distCoeffs.at<double>(0, i);
            if (i < distCoeffs.cols - 1) oss << ", ";
        }
        oss << "]\n";
        return oss.str();
    }
};

/**
 * @brief 标定配置
 */
struct CalibrationConfig {
    int boardWidth = 9;             // 棋盘宽度（内角点数）
    int boardHeight = 6;            // 棋盘高度（内角点数）
    float squareSize = 1.0f;        // 方格大小（实际物理尺寸，单位：mm）
    int minValidFrames = 20;        // 最少需要的有效帧数
    int maxFrames = 50;             // 最大采集帧数
    double minInterval = 1.0;       // 采集间隔（秒）
    bool useSubPixel = true;        // 是否使用亚像素精度
    bool enableUndistortion = true; // 是否启用去畸变
    std::string saveDirectory = "./calibration/"; // 保存目录
    
    bool isValid() const {
        return boardWidth > 0 && boardHeight > 0 && squareSize > 0 && 
               minValidFrames > 0 && maxFrames >= minValidFrames;
    }
};

/**
 * @brief 标定状态
 */
enum class CalibrationState {
    IDLE,           // 空闲状态
    COLLECTING,     // 采集中
    PROCESSING,     // 处理中
    COMPLETED,      // 完成
    FAILED          // 失败
};

/**
 * @brief 标定进度回调函数类型
 */
using CalibrationProgressCallback = std::function<void(CalibrationState state, 
                                                      int currentFrames, 
                                                      int totalFrames, 
                                                      const std::string& message)>;

/**
 * @brief 标定管理器 - 单例模式
 * 负责相机标定的所有功能，与其他组件保持低耦合
 */
class CalibrationManager {
public:
    /**
     * @brief 获取单例实例
     * @return 单例实例引用
     */
    static CalibrationManager& getInstance();
    
    /**
     * @brief 初始化标定管理器
     * @return 是否初始化成功
     */
    bool initialize();
    
    /**
     * @brief 开始标定
     * @param config 标定配置
     * @param callback 进度回调函数
     * @return 是否成功开始
     */
    bool startCalibration(const CalibrationConfig& config, 
                         CalibrationProgressCallback callback = nullptr);
    
    /**
     * @brief 停止标定
     */
    void stopCalibration();
    
    /**
     * @brief 处理输入帧（主要接口）
     * @param frame 输入帧
     * @return 是否成功处理
     */
    bool processFrame(const cv::Mat& frame);
    
    /**
     * @brief 处理Orbbec帧
     * @param frame Orbbec帧
     * @return 是否成功处理
     */
    bool processFrame(std::shared_ptr<ob::Frame> frame);
    
    /**
     * @brief 获取当前状态
     * @return 当前标定状态
     */
    CalibrationState getState() const { 
        std::lock_guard<std::mutex> lock(mutex_);
        return state_; 
    }
    
    /**
     * @brief 获取当前采集的帧数
     * @return 采集的帧数
     */
    int getCurrentFrameCount() const { 
        std::lock_guard<std::mutex> lock(mutex_);
        return static_cast<int>(imagePoints_.size()); 
    }
    
    /**
     * @brief 获取最后的标定结果
     * @return 标定结果
     */
    const CalibrationResult& getLastResult() const { 
        std::lock_guard<std::mutex> lock(mutex_);
        return lastResult_; 
    }
    
    /**
     * @brief 保存标定结果
     * @param result 标定结果
     * @param filename 文件名（不含扩展名）
     * @return 是否保存成功
     */
    bool saveCalibrationResult(const CalibrationResult& result, 
                              const std::string& filename = "camera_calibration");
    
    /**
     * @brief 加载标定结果
     * @param filename 文件名（不含扩展名）
     * @return 标定结果
     */
    CalibrationResult loadCalibrationResult(const std::string& filename = "camera_calibration");
    
    /**
     * @brief 应用去畸变
     * @param src 源图像
     * @param dst 目标图像
     * @param result 标定结果
     * @return 是否成功
     */
    static bool undistortImage(const cv::Mat& src, cv::Mat& dst, 
                              const CalibrationResult& result);
    
    /**
     * @brief 绘制棋盘角点
     * @param image 图像
     * @param corners 角点
     * @param found 是否找到角点
     * @return 绘制后的图像
     */
    static cv::Mat drawChessboardCorners(const cv::Mat& image, 
                                        const std::vector<cv::Point2f>& corners, 
                                        bool found);
    
    /**
     * @brief 设置全局进度回调
     * @param callback 回调函数
     */
    void setProgressCallback(CalibrationProgressCallback callback);
    
    /**
     * @brief 检查是否已初始化
     * @return 是否已初始化
     */
    bool isInitialized() const { return initialized_; }
    
    /**
     * @brief 停止标定管理器
     */
    void stop();

private:
    CalibrationManager() = default;
    ~CalibrationManager();
    CalibrationManager(const CalibrationManager&) = delete;
    CalibrationManager& operator=(const CalibrationManager&) = delete;
    
    /**
     * @brief 执行标定计算
     * @return 标定结果
     */
    CalibrationResult performCalibration();
    
    /**
     * @brief 转换Orbbec帧为OpenCV Mat
     * @param frame Orbbec帧
     * @return OpenCV Mat
     */
    cv::Mat convertFrameToMat(std::shared_ptr<ob::Frame> frame);
    
    /**
     * @brief 生成棋盘世界坐标点
     * @return 世界坐标点
     */
    std::vector<cv::Point3f> generateChessboardPoints();
    
    /**
     * @brief 检查是否应该采集当前帧
     * @return 是否应该采集
     */
    bool shouldCaptureFrame();

private:
    mutable std::mutex mutex_;                    // 线程同步
    std::atomic<bool> initialized_{false};       // 初始化标志
    
    CalibrationConfig config_;                    // 标定配置
    CalibrationState state_ = CalibrationState::IDLE; // 当前状态
    CalibrationProgressCallback progressCallback_; // 进度回调
    
    std::vector<std::vector<cv::Point2f>> imagePoints_;  // 图像角点
    std::vector<std::vector<cv::Point3f>> objectPoints_; // 世界坐标点
    cv::Size imageSize_;                          // 图像大小
    
    CalibrationResult lastResult_;                // 最后的标定结果
    std::chrono::steady_clock::time_point lastCaptureTime_; // 最后采集时间
};

} // namespace calibration 