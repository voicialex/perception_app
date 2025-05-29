#include "PerceptionSystem.hpp"
#include "Logger.hpp"
#include "ConfigHelper.hpp"
#include <chrono>
#include <thread>
#include <functional>

// 单例实现
PerceptionSystem& PerceptionSystem::getInstance() {
    static PerceptionSystem instance;
    return instance;
}

PerceptionSystem::PerceptionSystem()
    : commProxy_(CommunicationProxy::getInstance()) {
    LOG_DEBUG("PerceptionSystem created");
    registerStateHandlers();
}

PerceptionSystem::~PerceptionSystem() {
    cleanup();
    LOG_DEBUG("PerceptionSystem destroyed");
}

bool PerceptionSystem::initialize() {
    if(isInitialized_) {
        LOG_WARN("PerceptionSystem already initialized");
        return true;
    }

    LOG_INFO("Initializing PerceptionSystem...");
    
    // 初始化通信代理
    if(!commProxy_.initialize()) {
        LOG_ERROR("Failed to initialize CommunicationProxy");
        return false;
    }
    
    // 设置通信回调
    setupCommunicationCallbacks();
    
    // 创建图像接收器
    imageReceiver_ = std::make_unique<ImageReceiver>();
    
    // 初始化图像接收器
    if(!imageReceiver_->initialize()) {
        LOG_ERROR("Failed to initialize ImageReceiver");
        return false;
    }
    
    // 初始化后立即显示黑色无信号画面
    imageReceiver_->showNoSignalFrame();
    
    // 设置初始状态为待命
    currentState_ = SystemState::PENDING;
    
    isInitialized_ = true;
    LOG_INFO("PerceptionSystem initialized successfully");
    return true;
}

void PerceptionSystem::registerStateHandlers() {
    // 注册各状态对应的处理函数
    stateHandlers_[SystemState::RUNNING] = [this]() { handleRunningState(); };
    stateHandlers_[SystemState::PENDING] = [this]() { handlePendingState(); };
    stateHandlers_[SystemState::ERROR] = [this]() { handleErrorState(); };
    stateHandlers_[SystemState::CALIBRATING] = [this]() { handleCalibratingState(); };
    stateHandlers_[SystemState::UPGRADING] = [this]() { handleUpgradingState(); };
    stateHandlers_[SystemState::SHUTDOWN] = [this]() { handleShutdownState(); };
}

void PerceptionSystem::handleRunningState() {
    LOG_INFO("Handling RUNNING state event");
    
    // 启动图像接收器流处理
    if(imageReceiver_) {
        if(imageReceiver_->startStreaming()) {
            LOG_INFO("Image streaming started successfully");
        } else {
            LOG_ERROR("Failed to start image streaming, setting system to ERROR state");
            setState(SystemState::ERROR);
        }
    }
}

void PerceptionSystem::handlePendingState() {
    LOG_INFO("Handling PENDING state event");
    
    // 停止图像接收器的流处理，但保持窗口显示黑色背景
    if(imageReceiver_) {
        imageReceiver_->stopStreaming();
        // 显示无信号画面
        imageReceiver_->showNoSignalFrame();
    }
}

void PerceptionSystem::handleErrorState() {
    LOG_ERROR("Handling ERROR state event");
    
    // 停止图像接收器
    if(imageReceiver_) {
        imageReceiver_->stopStreaming();
        // 显示无信号画面
        imageReceiver_->showNoSignalFrame();
    }
}

void PerceptionSystem::handleCalibratingState() {
    LOG_INFO("Handling CALIBRATING state event");
    
    // 停止图像接收器
    if(imageReceiver_) {
        imageReceiver_->stopStreaming();
    }
    
    // 这里可以添加校准相关的代码
}

void PerceptionSystem::handleUpgradingState() {
    LOG_INFO("Handling UPGRADING state event");
    
    // 停止图像接收器
    if(imageReceiver_) {
        imageReceiver_->stopStreaming();
    }
    
    // 这里可以添加升级相关的代码
}

void PerceptionSystem::handleShutdownState() {
    LOG_INFO("Handling SHUTDOWN state event");
    
    // 停止图像接收器
    if(imageReceiver_) {
        imageReceiver_->stopStreaming();
    }

    // 停止整个系统
    stop();
}

void PerceptionSystem::run() {
    if(!isInitialized_) {
        LOG_ERROR("Cannot run PerceptionSystem: not initialized");
        return;
    }

    if(isRunning_) {
        LOG_WARN("PerceptionSystem already running");
        return;
    }

    LOG_INFO("Starting and running PerceptionSystem...");
    
    // 启动通信代理
    commProxy_.start();
    
    // 设置运行标志
    isRunning_ = true;
    shouldExit_ = false;
    
    // 检查设备管理器状态并等待设备连接
    if(imageReceiver_) {
        auto deviceState = imageReceiver_->getDeviceState();
        LOG_INFO("Current device state: ", static_cast<int>(deviceState));
        
        // 如果设备未连接，主动等待设备连接
        if(deviceState != DeviceManager::DeviceState::CONNECTED) {
            LOG_INFO("Waiting for device connection...");
            bool deviceConnected = imageReceiver_->waitForDevice(10000);  // 等待10秒
            if(deviceConnected) {
                LOG_INFO("Device connected successfully");
            } else {
                LOG_WARN("No device connected within timeout, continuing anyway...");
            }
            
            // 再次检查设备状态
            deviceState = imageReceiver_->getDeviceState();
            LOG_INFO("Device state after waiting: ", static_cast<int>(deviceState));
        }
    }
    
    // 设置初始系统状态为运行状态而不是待命状态
    setState(SystemState::RUNNING);
    
    // 启动图像接收器的运行循环（非阻塞）
    if(imageReceiver_) {
        imageReceiver_->run();
    }
    
    LOG_INFO("PerceptionSystem running with initial state: ", getStateName(currentState_));
    
    // 主循环 - 只用于保持程序运行，不再处理状态
    while(!shouldExit_) {
        try {
            // 短暂休眠，减少CPU使用
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            // 检查图像接收器窗口是否正常
            if(imageReceiver_ && currentState_ == SystemState::PENDING) {
                // 在待命状态下，确保始终显示无信号画面
                imageReceiver_->showNoSignalFrame();
            }
        }
        catch(const std::exception& e) {
            LOG_ERROR("Exception in main loop: ", e.what());
            setState(SystemState::ERROR);
        }
    }
    
    LOG_INFO("PerceptionSystem main loop exited");
}

void PerceptionSystem::stop() {
    if(!isRunning_) {
        return;
    }

    LOG_INFO("Stopping PerceptionSystem...");
    
    // 设置退出标志
    shouldExit_ = true;
    isRunning_ = false;
    
    // 停止图像接收器
    if(imageReceiver_) {
        imageReceiver_->stopStreaming();
    }
    
    // 停止通信代理
    commProxy_.stop();
    
    LOG_INFO("PerceptionSystem stopped");
}

bool PerceptionSystem::setState(SystemState newState) {
    SystemState oldState = currentState_;
    
    // 检查状态转换是否有效
    if(!isValidStateTransition(oldState, newState)) {
        LOG_ERROR("Invalid state transition: ", getStateName(oldState), 
                 " -> ", getStateName(newState));
        return false;
    }
    
    // 执行状态转换
    currentState_ = newState;
    
    // 处理状态转换
    handleStateTransition(oldState, newState);
    
    // 发送统一的状态报告
    std::string statusMessage;
    if(newState == SystemState::ERROR) {
        statusMessage = "SYSTEM_ERROR";
        commProxy_.sendMessage(
            CommunicationProxy::MessageType::ERROR,
            statusMessage
        );
    } else {
        statusMessage = "SYSTEM_" + getStateName(newState);
        commProxy_.sendMessage(
            CommunicationProxy::MessageType::STATUS_REPORT,
            statusMessage
        );
    }
    
    LOG_INFO("发送状态报告: ", statusMessage);
    
    return true;
}

std::string PerceptionSystem::getStateName(SystemState state) {
    switch(state) {
        case SystemState::UNKNOWN:     return "UNKNOWN";
        case SystemState::PENDING:     return "PENDING";
        case SystemState::RUNNING:     return "RUNNING";
        case SystemState::ERROR:       return "ERROR";
        case SystemState::CALIBRATING: return "CALIBRATING";
        case SystemState::UPGRADING:   return "UPGRADING";
        case SystemState::SHUTDOWN:    return "SHUTDOWN";
        default:                       return "INVALID_STATE";
    }
}

bool PerceptionSystem::isValidStateTransition(SystemState oldState, SystemState newState) {
    // 如果状态相同，不需要转换
    if(oldState == newState) {
        return false;
    }
    
    // 从错误状态只能转到待命状态
    if(oldState == SystemState::ERROR && newState != SystemState::PENDING) {
        return false;
    }
    
    // 从关闭状态不能转换到其他状态
    if(oldState == SystemState::SHUTDOWN) {
        return false;
    }
    
    // 从升级状态只能转到待命或错误状态
    if(oldState == SystemState::UPGRADING && 
       newState != SystemState::PENDING && 
       newState != SystemState::ERROR) {
        return false;
    }
    
    // 其他状态转换都是允许的
    return true;
}

void PerceptionSystem::handleStateTransition(SystemState oldState, SystemState newState) {
    // 查找并执行新状态对应的处理函数
    auto it = stateHandlers_.find(newState);
    if (it != stateHandlers_.end()) {
        LOG_INFO("System state changed: ", getStateName(oldState), " -> ", getStateName(newState));
        it->second(); // 调用状态处理函数
    } else {
        LOG_WARN("No handler registered for state: ", getStateName(newState));
    }
}

void PerceptionSystem::setupCommunicationCallbacks() {
    // 注册连接状态变化回调 - 使用std::bind直接绑定成员函数
    commProxy_.registerConnectionCallback(
        std::bind(&PerceptionSystem::handleConnectionStateChanged, this, std::placeholders::_1)
    );

    // 注册命令消息回调
    commProxy_.registerCallback(
        CommunicationProxy::MessageType::COMMAND,
        std::bind(&PerceptionSystem::handleCommunicationMessage, this, std::placeholders::_1)
    );
    
    // 注册心跳消息回调 - 收到心跳请求后立即回复
    commProxy_.registerCallback(
        CommunicationProxy::MessageType::HEARTBEAT,
        std::bind(&PerceptionSystem::handleHeartBeatMessage, this, std::placeholders::_1)
    );
}

void PerceptionSystem::handleCommunicationMessage(const CommunicationProxy::Message& message) {
    LOG_DEBUG("Received communication message: ", message.content);
    
    // 处理状态切换命令
    if(message.content == "START_RUNNING") {
        // 状态转换会触发耗时操作，但此时已在线程池中执行
        setState(SystemState::RUNNING);
    }
    else if(message.content == "START_PENDING") {
        setState(SystemState::PENDING);
    }
    else if(message.content == "START_CALIBRATION") {
        setState(SystemState::CALIBRATING);
    }
    else if(message.content == "START_STANDBY") {
        // 将STANDBY命令映射到PENDING状态
        setState(SystemState::PENDING);
    }
    else if(message.content == "START_UPGRADE") {
        setState(SystemState::UPGRADING);
    }
    else if(message.content == "SHUTDOWN") {
        setState(SystemState::SHUTDOWN);
    }
    else if(message.content == "REPORT_ERROR") {
        setState(SystemState::ERROR);
    }
    else if(message.content == "GET_STATUS") {
        // 状态查询不涉及状态转换，可以快速响应
        // 发送状态报告，不考虑连接状态（如果未连接则丢弃）
        commProxy_.sendMessage(
            CommunicationProxy::MessageType::STATUS_REPORT,
            "CURRENT_STATE:" + getStateName(currentState_)
        );
    }
    else if(message.content == "TAKE_SNAPSHOT" && imageReceiver_) {
        LOG_INFO("Taking snapshot command received");
        // 直接修改全局配置而不是调用已删除的方法
        ConfigHelper::getInstance().saveConfig.enableDump = true;
    }
    else if(message.content == "START_CAPTURE") {
        LOG_INFO("Start capturing command received");
        // 直接修改全局配置
        ConfigHelper::getInstance().saveConfig.enableDump = true;
        LOG_INFO("Data capture started");
        commProxy_.sendMessage(
            CommunicationProxy::MessageType::STATUS_REPORT,
            "CAPTURE_STARTED"
        );
    }
    else if(message.content == "STOP_CAPTURE") {
        LOG_INFO("Stop capturing command received");
        // 直接修改全局配置
        ConfigHelper::getInstance().saveConfig.enableDump = false;
        LOG_INFO("Data capture stopped");
        commProxy_.sendMessage(
            CommunicationProxy::MessageType::STATUS_REPORT,
            "CAPTURE_STOPPED"
        );
    }
    else {
        LOG_WARN("Unknown command: ", message.content);
    }
}

void PerceptionSystem::handleHeartBeatMessage(const CommunicationProxy::Message& message)
{
    // 收到心跳消息，立即回复心跳响应
    LOG_DEBUG("收到心跳请求: ", message.content);
    
    // 解析心跳请求，检查是否包含PING前缀
    std::string requestType = message.content;
    std::string requestData;
    
    // 分离前缀和数据部分
    size_t colonPos = message.content.find(':');
    if (colonPos != std::string::npos) {
        requestType = message.content.substr(0, colonPos);
        requestData = message.content.substr(colonPos + 1);
    }
    
    // 只响应PING开头的心跳请求
    if (requestType == "PING") {
        // 回复心跳请求，包含当前状态信息和原始请求数据
        LOG_DEBUG("回复心跳请求: PING:", requestData, " -> PONG:", requestData, ":", getStateName(currentState_));
        commProxy_.sendMessage(
            CommunicationProxy::MessageType::HEARTBEAT,
            "PONG:" + requestData + ":" + getStateName(currentState_)
        );
    }
}

// 处理连接状态变化
void PerceptionSystem::handleConnectionStateChanged(CommunicationProxy::ConnectionState newState) {
    
    // 将状态代码转换为更可读的字符串
    std::string stateStr;
    switch (newState) {
        case CommunicationProxy::ConnectionState::DISCONNECTED:
            stateStr = "断开连接";
            break;
        case CommunicationProxy::ConnectionState::CONNECTING:
            stateStr = "连接中";
            break;
        case CommunicationProxy::ConnectionState::CONNECTED:
            stateStr = "已连接";
            break;
        default:
            stateStr = "未知状态";
    }
    
    LOG_INFO("通信连接状态变化: ", stateStr, " (", static_cast<int>(newState), ")");
    
    // 当连接建立时，主动发送当前状态
    if (newState == CommunicationProxy::ConnectionState::CONNECTED) {
        LOG_INFO("通信连接已建立，发送当前状态...");
    }
} 

void PerceptionSystem::cleanup() {
    LOG_DEBUG("Cleaning up PerceptionSystem resources...");
    
    // 停止所有活动
    stop();
    
    // 清理图像接收器
    if(imageReceiver_) {
        imageReceiver_.reset();
    }
    
    LOG_DEBUG("PerceptionSystem cleanup completed");
}
