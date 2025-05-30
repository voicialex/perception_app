#pragma once

#include <memory>
#include <atomic>
#include <mutex>
#include <string>
#include <map>
#include <functional>
#include <queue>
#include "ImageReceiver.hpp"
#include "CommunicationProxy.hpp"
#include "inference/InferenceManager.hpp"
#include "calibration/CalibrationManager.hpp"

/**
 * @brief 感知系统 - 整个相机系统的主控制类
 * 
 * 负责系统状态管理、组件协调和通信接口
 * 采用单例模式设计，集成推理和标定功能
 */
class PerceptionSystem {
public:
    /**
     * @brief 系统状态枚举
     */
    enum class SystemState {
        UNKNOWN,    // 未知状态
        PENDING,    // 待命状态
        RUNNING,    // 运行状态
        ERROR,      // 错误状态
        CALIBRATING,// 校准状态
        UPGRADING,  // 升级状态
        SHUTDOWN    // 关闭状态
    };

    /**
     * @brief 状态处理函数类型
     */
    using StateHandler = std::function<void()>;

    /**
     * @brief 获取单例实例
     * @return 单例实例引用
     */
    static PerceptionSystem& getInstance();
    
    /**
     * @brief 初始化感知系统
     * @return true if successful
     */
    bool initialize();
    
    /**
     * @brief 停止感知系统
     */
    void stop();
    
    /**
     * @brief 运行主循环（包含启动逻辑）
     */
    void run();
    
    /**
     * @brief 设置系统状态
     * @param newState 新状态
     * @return true if successful
     */
    bool setState(SystemState newState);
    
    /**
     * @brief 获取当前系统状态
     * @return 当前状态
     */
    SystemState getState() const { return currentState_; }
    
    /**
     * @brief 获取状态名称
     * @param state 状态枚举
     * @return 状态名称字符串
     */
    static std::string getStateName(SystemState state);
    
    /**
     * @brief 处理帧数据（由 ImageReceiver 调用）
     * @param frame Orbbec帧数据
     * @param frameType 帧类型
     */
    void processFrame(std::shared_ptr<ob::Frame> frame, OBFrameType frameType);
    
    /**
     * @brief 获取推理管理器
     * @return 推理管理器引用
     */
    inference::InferenceManager& getInferenceManager() { 
        return inference::InferenceManager::getInstance(); 
    }
    
    /**
     * @brief 获取标定管理器
     * @return 标定管理器引用
     */
    calibration::CalibrationManager& getCalibrationManager() { 
        return calibration::CalibrationManager::getInstance(); 
    }
    
    /**
     * @brief 启动推理功能
     * @return 是否成功启动
     */
    bool enableInference();
    
    /**
     * @brief 停止推理功能
     */
    void disableInference();
    
    /**
     * @brief 启动标定功能
     * @return 是否成功启动
     */
    bool enableCalibration();
    
    /**
     * @brief 停止标定功能
     */
    void disableCalibration();
    
    /**
     * @brief 检查推理是否启用
     * @return 是否启用
     */
    bool isInferenceEnabled() const { return inferenceEnabled_; }
    
    /**
     * @brief 检查标定是否启用
     * @return 是否启用
     */
    bool isCalibrationEnabled() const { return calibrationEnabled_; }

private:
    /**
     * @brief 私有构造函数，实现单例模式
     */
    PerceptionSystem();
    
    /**
     * @brief 私有析构函数
     */
    ~PerceptionSystem();
    
    // 禁止拷贝和赋值
    PerceptionSystem(const PerceptionSystem&) = delete;
    PerceptionSystem& operator=(const PerceptionSystem&) = delete;
    
    /**
     * @brief 检查状态转换是否有效
     * @param oldState 旧状态
     * @param newState 新状态
     * @return true if valid
     */
    bool isValidStateTransition(SystemState oldState, SystemState newState);
    
    /**
     * @brief 处理状态转换
     * @param oldState 旧状态
     * @param newState 新状态
     */
    void handleStateTransition(SystemState oldState, SystemState newState);
    
    /**
     * @brief 设置通信回调
     */
    void setupCommunicationCallbacks();
    
    /**
     * @brief 处理通信消息
     * @param message 消息对象
     */
    void handleCommunicationMessage(const CommunicationProxy::Message& message);
    
    void handleHeartBeatMessage(const CommunicationProxy::Message& message);

    /**
     * @brief 清理资源
     */
    void cleanup();
    
    /**
     * @brief 处理RUNNING状态的事件
     */
    void handleRunningState();
    
    /**
     * @brief 处理PENDING状态的事件
     */
    void handlePendingState();
    
    /**
     * @brief 处理ERROR状态的事件
     */
    void handleErrorState();
    
    /**
     * @brief 处理CALIBRATING状态的事件
     */
    void handleCalibratingState();
    
    /**
     * @brief 处理UPGRADING状态的事件
     */
    void handleUpgradingState();
    
    /**
     * @brief 处理SHUTDOWN状态的事件
     */
    void handleShutdownState();
    
    /**
     * @brief 注册所有状态处理函数
     */
    void registerStateHandlers();

    /**
     * @brief 处理连接状态变化
     * @param newState 新状态
     */
    void handleConnectionStateChanged(CommunicationProxy::ConnectionState newState);
    
    /**
     * @brief 初始化推理系统
     * @return 是否成功
     */
    bool initializeInferenceSystem();
    
    /**
     * @brief 初始化标定系统
     * @return 是否成功
     */
    bool initializeCalibrationSystem();
    
    /**
     * @brief 处理推理结果
     * @param modelName 模型名称
     * @param image 输入图像
     * @param result 推理结果
     */
    void handleInferenceResult(const std::string& modelName, 
                              const cv::Mat& image,
                              std::shared_ptr<inference::InferenceResult> result);
    
    /**
     * @brief 处理标定进度
     * @param state 标定状态
     * @param currentFrames 当前帧数
     * @param totalFrames 总帧数
     * @param message 消息
     */
    void handleCalibrationProgress(calibration::CalibrationState state,
                                  int currentFrames,
                                  int totalFrames,
                                  const std::string& message);
    
    // 成员变量
    CommunicationProxy& commProxy_;             ///< 通信代理引用
    std::unique_ptr<ImageReceiver> imageReceiver_; ///< 图像接收器
    
    SystemState currentState_ = SystemState::UNKNOWN; ///< 当前系统状态
    
    std::atomic<bool> isInitialized_{false};  ///< 初始化标志
    std::atomic<bool> isRunning_{false};      ///< 运行标志
    std::atomic<bool> shouldExit_{false};     ///< 退出标志
    
    std::mutex callbackMutex_;                ///< 回调锁
    
    // 状态处理函数映射
    std::map<SystemState, StateHandler> stateHandlers_;
    
    // 推理和标定功能状态
    std::atomic<bool> inferenceEnabled_{false};   ///< 推理功能启用标志
    std::atomic<bool> calibrationEnabled_{false}; ///< 标定功能启用标志
}; 