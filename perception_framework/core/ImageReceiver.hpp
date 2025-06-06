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
#include <functional>
#include <opencv2/opencv.hpp>

#include "libobsensor/ObSensor.hpp"
#include "libobsensor/hpp/Error.hpp"
#include "Logger.hpp"
#include "ConfigHelper.hpp"
#include "utils.hpp"
#include "utils_opencv.hpp"
#include "MetadataHelper.hpp"
#include "DumpHelper.hpp"
#include "DeviceManager.hpp"
#include "ThreadPool.hpp"

/**
 * @brief 图像接收器 - 主要的相机数据处理类
 * 负责数据流管理、图像渲染、用户交互等功能
 */
class ImageReceiver {
public:
    /**
     * @brief 流状态枚举
     */
    enum class StreamState {
        IDLE,       // 空闲状态
        RUNNING,    // 运行状态
        ERROR       // 错误状态
    };

    /**
     * @brief 帧处理回调函数类型
     */
    using FrameProcessCallback = std::function<void(std::shared_ptr<ob::Frame>, OBFrameType)>;

    ImageReceiver();
    ~ImageReceiver();

    /**
     * @brief 初始化图像接收器
     * @return true if successful
     */
    bool initialize();

    /**
     * @brief 运行主循环（UI渲染和事件处理）
     */
    void run();

    /**
     * @brief 停止所有活动并退出
     */
    void stop();
    
    /**
     * @brief 开始流处理（合并了管道设置和启动）
     * @return 是否成功启动
     */
    bool startStreaming();

    /**
     * @brief 停止流处理
     */
    void stopStreaming();
    
    /**
     * @brief 重启流处理
     * @return 是否成功重启
     */
    bool restartStreaming();

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
    
    /**
     * @brief 获取设备状态
     * @return 当前设备状态
     */
    DeviceManager::DeviceState getDeviceState() const {
        if (deviceManager_) {
            return deviceManager_->getDeviceState();
        }
        return DeviceManager::DeviceState::DISCONNECTED;
    }
    
    /**
     * @brief 获取流状态
     * @return 当前流状态
     */
    StreamState getStreamState() const {
        return streamState_;
    }
    
    /**
     * @brief 等待设备连接
     * @param timeoutMs 超时时间(毫秒)，0表示无限等待
     * @return 是否成功连接
     */
    bool waitForDevice(int timeoutMs = 0) {
        if (deviceManager_) {
            return deviceManager_->waitForDevice(timeoutMs);
        }
        return false;
    }
    
    /**
     * @brief 设置帧处理回调（由 PerceptionSystem 调用）
     * @param callback 回调函数
     */
    void setFrameProcessCallback(FrameProcessCallback callback) {
        frameProcessCallback_ = callback;
    }

private:
    // 核心功能方法
    void processFrameSet(std::shared_ptr<ob::FrameSet> frameset);
    void processFrame(std::shared_ptr<ob::Frame> frame);
    void handleError(ob::Error &e);
    void cleanup();
    
    // 流管理相关内部方法
    bool setupPipelines();
    bool startPipelines();
    void stopPipelines();
    
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
    void resetPerformanceStats();
    
    // 无信号画面
    void createNoSignalFrame();

    // 并行处理
    void processFrameSetParallel(std::shared_ptr<ob::FrameSet> frameset);

    // 清理已完成的任务
    void cleanupCompletedTasks();

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
    std::atomic<StreamState> streamState_{StreamState::IDLE};
    
    // 性能统计
    struct PerformanceStats {
        std::atomic<uint64_t> frameCount{0};
        std::atomic<uint64_t> totalFrames{0};
        std::chrono::steady_clock::time_point startTime;
        std::chrono::steady_clock::time_point lastStatsTime;
        double currentFPS{0.0};
        double averageFPS{0.0};
        
        // 帧处理时间统计
        std::atomic<uint64_t> totalProcessingTime{0};  // 总处理时间(毫秒)
        std::atomic<uint64_t> processedFramesCount{0}; // 统计处理时间的帧数
        std::atomic<double> currentProcessingTime{0.0}; // 当前帧处理时间(毫秒)
        double minProcessingTime{std::numeric_limits<double>::max()}; // 最小处理时间(毫秒)
        double maxProcessingTime{0.0};                 // 最大处理时间(毫秒)
        double avgProcessingTime{0.0};                 // 平均处理时间(毫秒)
    } performanceStats_;

    // 热插拔状态
    std::atomic<int> reconnectAttempts_{0};
    std::chrono::steady_clock::time_point lastDisconnectTime_;
    
    // 无信号画面
    cv::Mat noSignalMat_;
    std::atomic<bool> showingNoSignalFrame_{false};
    std::chrono::steady_clock::time_point lastFrameTime_;
    std::atomic<int> noFrameCounter_{0};

    // 线程池
    std::unique_ptr<utils::ThreadPool> threadPool_;
    
    // 并行处理配置
    bool enableParallelProcessing_ = true;  // 启用并行处理
    int threadPoolSize_ = 4;                // 线程池大小
    
    // 跟踪并行任务完成
    std::mutex futuresMutex_;
    std::vector<std::future<void>> frameFutures_;
    
    // 帧处理回调（与 PerceptionSystem 通信）
    FrameProcessCallback frameProcessCallback_;
};