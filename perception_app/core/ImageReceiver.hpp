#pragma once

#include <chrono>
#include <fstream>
#include <iomanip>
#include <map>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>
#include <atomic>
#include <condition_variable>
#include <memory>

#include "libobsensor/ObSensor.hpp"
#include "libobsensor/hpp/Error.hpp"
#include "utils.hpp"
#include "utils_opencv.hpp"
#include "MetadataHelper.hpp"
#include "DumpHelper.hpp"
#include "ConfigHelper.hpp"
#include "Logger.hpp"
#include "DeviceManager.hpp"
#include <opencv2/opencv.hpp>

/**
 * @brief 图像接收器 - 主要的相机数据处理类
 * 负责数据流管理、图像渲染、用户交互等功能
 */
class ImageReceiver {
public:
    ImageReceiver();
    ~ImageReceiver();

    /**
     * @brief 初始化图像接收器
     * @return true if successful
     */
    bool initialize();

    /**
     * @brief 运行主循环
     */
    void run();

    /**
     * @brief 停止运行
     */
    void stop();
    
    /**
     * @brief 开始捕获
     */
    void startCapture();
    
    /**
     * @brief 停止捕获
     */
    void stopCapture();
    
    /**
     * @brief 设置是否启用帧保存
     * @param enabled 是否启用
     */
    void setDumpEnabled(bool enabled);
    
    /**
     * @brief 设置是否启用元数据打印
     * @param enabled 是否启用
     */
    void setMetadataEnabled(bool enabled);

    /**
     * @brief 设置数据流管道
     * @return 是否成功设置管道
     */
    bool setupPipelines();
    
    /**
     * @brief 启动数据流管道
     * @return 是否成功启动管道
     */
    bool startPipelines();
    
    /**
     * @brief 停止数据流管道
     */
    void stopPipelines();

    // 热插拔相关接口
    void enableHotPlug(bool enable = true);
    void rebootCurrentDevice();
    void printConnectedDevices();
    
    /**
     * @brief 显示无信号画面
     */
    void showNoSignalFrame();
    
    /**
     * @brief 检查是否正在显示无信号画面
     * @return 是否正在显示无信号画面
     */
    bool isNoSignalFrameShowing() const;

private:
    // 核心功能方法
    void processFrameSet(std::shared_ptr<ob::FrameSet> frameset);
    void processFrame(std::shared_ptr<ob::Frame> frame);
    void handleError(ob::Error &e);
    void cleanup();
    
    // 数据流管理
    bool isVideoSensorTypeEnabled(OBSensorType sensorType);
    
    // 设备事件处理
    void onDeviceStateChanged(DeviceManager::DeviceState oldState, 
                             DeviceManager::DeviceState newState, 
                             std::shared_ptr<ob::Device> device);
    
    // 用户交互
    void setupKeyboardCallbacks();
    void handleKeyPress(int key);
    
    // 渲染相关
    void renderFrames();
    void updateWindowTitle();
    
    // 性能统计
    void updatePerformanceStats();
    void printPerformanceStats();
    
    // 无信号画面
    void createNoSignalFrame();

private:
    // 设备管理
    std::unique_ptr<DeviceManager> deviceManager_;
    
    // 数据流管道
    std::shared_ptr<ob::Pipeline> mainPipeline_;
    std::shared_ptr<ob::Config> config_;
    std::shared_ptr<ob::Pipeline> imuPipeline_;
    std::shared_ptr<ob::Config> imuConfig_;

    // 渲染窗口
    std::unique_ptr<ob_smpl::CVWindow> window_;

    // 线程同步
    std::mutex frameMutex_;
    std::mutex imuFrameMutex_;
    std::condition_variable frameCondition_;
    
    // 帧数据存储
    std::map<OBFrameType, std::shared_ptr<ob::Frame>> frameMap_;
    std::map<OBFrameType, std::shared_ptr<ob::Frame>> imuFrameMap_;

    // 状态管理
    std::atomic<bool> shouldExit_{false};
    std::atomic<bool> pipelinesRunning_{false};
    std::atomic<bool> isInitialized_{false};
    
    // 功能开关
    std::atomic<bool> dumpEnabled_{false};
    std::atomic<bool> metadataEnabled_{false};
    
    // 性能统计
    struct PerformanceStats {
        std::atomic<uint64_t> frameCount{0};
        std::atomic<uint64_t> totalFrames{0};
        std::chrono::steady_clock::time_point startTime;
        std::chrono::steady_clock::time_point lastStatsTime;
        double currentFPS{0.0};
        double averageFPS{0.0};
    } performanceStats_;

    // 热插拔状态
    std::atomic<int> reconnectAttempts_{0};
    std::chrono::steady_clock::time_point lastDisconnectTime_;
    
    // 无信号画面
    cv::Mat noSignalMat_;
    std::atomic<bool> showingNoSignalFrame_{false};
    std::chrono::steady_clock::time_point lastFrameTime_;
    std::atomic<int> noFrameCounter_{0};
};