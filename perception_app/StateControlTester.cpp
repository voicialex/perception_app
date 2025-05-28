// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include <iostream>
#include <memory>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>
#include <string>
#include "CommunicationProxy.hpp"
#include "Logger.hpp"

// 全局变量用于信号处理
std::atomic<bool> g_exitRequested{false};

/**
 * @brief 信号处理函数
 * @param signal 信号类型
 */
void signalHandler(int signal) {
    LOG_INFO("收到信号 ", signal, ", 正在关闭...");
    g_exitRequested = true;
    
    // 给程序一些时间来优雅退出
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // 如果程序没有及时退出，则强制退出
    LOG_WARN("强制退出程序");
    _exit(signal);
}

/**
 * @brief 简化版状态控制测试程序
 */
class StateController {
public:
    StateController() : commProxy_(CommunicationProxy::getInstance()) {
        LOG_INFO("StateController 已创建");
    }
    
    ~StateController() {
        stop();
        LOG_INFO("StateController 已销毁");
    }
    
    bool initialize() {
        LOG_INFO("正在初始化通信客户端...");
        
        // 初始化通信代理 - 作为客户端
        if(!commProxy_.initialize("/tmp/orbbec_camera", CommunicationProxy::CommRole::CLIENT)) {
            LOG_ERROR("初始化通信失败，请确保服务端程序已运行且有足够权限");
            return false;
        }
        
        // 设置通信回调
        setupCallbacks();
        
        state_ = ControllerState::INITIALIZED;
        
        LOG_INFO("初始化成功");
        
        return true;
    }
    
    void run() {
        if(state_ == ControllerState::UNINITIALIZED) {
            LOG_ERROR("未初始化，无法运行");
            return;
        }
        
        LOG_INFO("正在启动...");
        
        // 启动通信代理
        commProxy_.start();
        
        state_ = ControllerState::RUNNING;
        
        // 立即发送一次心跳消息，激活通信连接
        sendHeartbeat();
        
        // 启动心跳线程
        startHeartbeatThread();
        
        LOG_INFO("已启动，进入主循环");
        
        // 显示操作菜单
        printMenu();
        
        // 主循环
        while(state_ == ControllerState::RUNNING && !g_exitRequested) {
            // 处理用户输入
            processUserInput();
            
            // 短暂休眠，减少CPU使用
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        
        LOG_INFO("主循环已退出");
    }
    
    void stop() {
        if(state_ != ControllerState::RUNNING) {
            return;
        }
        
        LOG_INFO("正在停止...");
        
        state_ = ControllerState::STOPPING;
        
        // 等待心跳线程结束
        if (heartbeatThread_.joinable()) {
            heartbeatThread_.join();
        }
        
        // 停止通信代理
        commProxy_.stop();
        
        LOG_INFO("已停止");
    }
    
private:
    void printMenu() {
        LOG_INFO("=== 感知系统状态控制器 ===");
        LOG_INFO("可用命令:");
        LOG_INFO("  1 - 切换到 运行(RUNNING) 状态");
        LOG_INFO("  2 - 切换到 待命(PENDING) 状态");
        LOG_INFO("  3 - 切换到 校准(CALIBRATING) 状态");
        LOG_INFO("  4 - 切换到 待命(PENDING) 状态 (通过 STANDBY 命令)");
        LOG_INFO("  5 - 获取当前状态");
        LOG_INFO("  6 - 拍照");
        LOG_INFO("  0 或 q - 退出");
    }
    
    void processUserInput() {
        if(std::cin.peek() != EOF) {
            char input;
            std::cin >> input;
            
            switch(input) {
                case '1':
                    LOG_INFO("切换到 运行(RUNNING) 状态");
                    sendCommand("START_RUNNING");
                    break;
                    
                case '2':
                    LOG_INFO("切换到 待命(PENDING) 状态");
                    sendCommand("START_PENDING");
                    break;
                    
                case '3':
                    LOG_INFO("切换到 校准(CALIBRATING) 状态");
                    sendCommand("START_CALIBRATION");
                    break;
                    
                case '4':
                    LOG_INFO("切换到 待命(PENDING) 状态 (通过 STANDBY 命令)");
                    sendCommand("START_STANDBY");
                    break;
                    
                case '5':
                    LOG_INFO("请求当前状态");
                    sendCommand("GET_STATUS");
                    break;
                    
                case '6':
                    LOG_INFO("拍照");
                    sendCommand("TAKE_SNAPSHOT");
                    break;
                    
                case '0': case 'q': case 'Q':
                    LOG_INFO("退出程序...");
                    state_ = ControllerState::STOPPING;
                    break;
                    
                default:
                    LOG_INFO("未知命令: ", input);
                    printMenu(); // 显示帮助信息
                    break;
            }
        }
    }
    
    void setupCallbacks() {
        // 注册状态报告消息回调
        commProxy_.registerCallback(
            CommunicationProxy::MessageType::STATUS_REPORT,
            [this](const CommunicationProxy::Message& message) {
                handleStatusReport(message);
            }
        );
        
        // 注册错误消息回调
        commProxy_.registerCallback(
            CommunicationProxy::MessageType::ERROR,
            [this](const CommunicationProxy::Message& message) {
                LOG_ERROR("收到错误消息: ", message.content);
            }
        );
        
        // 注册心跳回复消息回调
        commProxy_.registerCallback(
            CommunicationProxy::MessageType::HEARTBEAT,
            [this](const CommunicationProxy::Message& message) {
                handleHeartbeat(message);
            }
        );
        
        // 注册连接状态变化回调
        commProxy_.registerConnectionCallback(
            [this](CommunicationProxy::ConnectionState newState) {
                handleConnectionStateChanged(newState);
            }
        );
    }
    
    void handleStatusReport(const CommunicationProxy::Message& message) {
        LOG_INFO("收到状态报告: ", message.content);
        
        // 更新连接状态
        isConnected_ = true;
        
        // 简化状态解析
        if(message.content.find("CURRENT_STATE:") != std::string::npos) {
            currentState_ = message.content.substr(14);
            LOG_INFO("当前系统状态: ", currentState_);
        }
        else if(message.content.find("STATE_CHANGED:") != std::string::npos) {
            currentState_ = message.content.substr(14);
            LOG_INFO("系统状态已更改为: ", currentState_);
        }
        else if(message.content.find("SYSTEM_STARTED:") != std::string::npos) {
            currentState_ = message.content.substr(15);
            LOG_INFO("系统启动状态: ", currentState_);
        }
    }
    
    void handleHeartbeat(const CommunicationProxy::Message& message) {
        // 检查是否是服务端的心跳回复
        if (message.content.find("PONG:") != std::string::npos || 
            message.content == "PONG") {
            lastHeartbeatTime_ = std::chrono::steady_clock::now();
            heartbeatReceived_ = true;
            
            // 更新连接状态
            isConnected_ = true;
            
            // 解析PONG消息中的状态信息
            std::string responseData;
            std::string systemState;
            
            // 从PONG:timestamp:STATE格式解析
            if (message.content.find("PONG:") != std::string::npos) {
                std::string content = message.content.substr(5); // 跳过"PONG:"
                size_t colonPos = content.find(':');
                if (colonPos != std::string::npos) {
                    // 提取时间戳和状态
                    responseData = content.substr(0, colonPos);
                    systemState = content.substr(colonPos + 1);
                    
                    // 更新系统状态
                    currentState_ = systemState;
                }
            }
        }
    }
    
    void sendCommand(const std::string& command) {
        // 允许数据丢失，只有在连接状态下才发送命令
        if (!isConnected_) {
            LOG_WARN("未连接到服务器，命令 \"", command, "\" 被丢弃");
            return;
        }
        
        LOG_INFO("发送命令: ", command);
        if (!commProxy_.sendMessage(CommunicationProxy::MessageType::COMMAND, command)) {
            LOG_ERROR("发送命令失败");
            isConnected_ = false;
        }
    }
    
    void startHeartbeatThread() {
        heartbeatThread_ = std::thread([this]() {
            LOG_INFO("心跳线程已启动");
            
            // 先等待短暂时间让初始化完成
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            // 发送第一次心跳
            sendHeartbeat();
            
            while (state_ == ControllerState::RUNNING && !g_exitRequested) {
                // 检查连接状态
                checkConnection();
                
                // 等待心跳间隔时间（5秒）或直到程序退出
                for (int i = 0; i < 50 && state_ == ControllerState::RUNNING && !g_exitRequested; i++) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
                
                // 发送定期心跳
                sendHeartbeat();
            }
            
            LOG_INFO("心跳线程已停止");
        });
    }
    
    void sendHeartbeat() {
        // 使用当前毫秒时间作为时间戳，确保每次都不同
        auto timestamp = std::to_string(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
                
        // 发送心跳消息
        if (!commProxy_.sendMessage(CommunicationProxy::MessageType::HEARTBEAT, "PING:" + timestamp)) {
            isConnected_ = false;
            LOG_WARN("心跳消息发送失败，连接可能已断开");
        }
    }
    
    void checkConnection() {
        auto now = std::chrono::steady_clock::now();
        
        // 如果已收到过心跳，检查最后一次心跳时间
        if (heartbeatReceived_) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                now - lastHeartbeatTime_).count();
            
            // 如果超过10秒没有收到心跳，认为连接断开
            if (elapsed > 20 && isConnected_) {
                isConnected_ = false;
                LOG_WARN("连接断开: ", elapsed, " 秒未收到心跳回复");
            }
        } else if (isConnected_) {
            isConnected_ = false;
            LOG_WARN("未收到心跳回复，可能未连接到服务器");
        }
    }
    
    void handleConnectionStateChanged(CommunicationProxy::ConnectionState newState) {
        std::string stateStr;
        switch (newState) {
            case CommunicationProxy::ConnectionState::DISCONNECTED:
                stateStr = "断开连接";
                isConnected_ = false;
                break;
            case CommunicationProxy::ConnectionState::CONNECTING:
                stateStr = "连接中";
                break;
            case CommunicationProxy::ConnectionState::CONNECTED:
                stateStr = "已连接";
                isConnected_ = true;
                
                // 立即发送心跳和状态查询激活连接
                LOG_INFO("连接建立，立即发送心跳和状态查询...");
                
                // 使用独立线程发送，避免回调中嵌套发送可能导致的问题
                std::thread([this]() {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    sendHeartbeat();
                    std::this_thread::sleep_for(std::chrono::milliseconds(20));
                    sendCommand("GET_STATUS");
                }).detach();
                
                break;
            default:
                stateStr = "未知状态";
                break;
        }
        
        LOG_INFO("连接状态变化: ", stateStr);
    }
    
    // 内部状态枚举
    enum class ControllerState {
        UNINITIALIZED,   // 未初始化
        INITIALIZED,     // 已初始化但未运行
        RUNNING,         // 运行中
        STOPPING         // 正在停止
    };

    CommunicationProxy& commProxy_;
    std::atomic<ControllerState> state_{ControllerState::UNINITIALIZED};
    
    // 连接状态
    std::atomic<bool> isConnected_{false};
    std::thread heartbeatThread_;
    
    // 当前系统状态
    std::string currentState_{"UNKNOWN"};
    
    // 心跳相关
    std::chrono::steady_clock::time_point lastHeartbeatTime_;
    std::atomic<bool> heartbeatReceived_{false};
};

/**
 * @brief 主函数
 */
int main() {
    try {
        // 设置信号处理
        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);
        
        LOG_INFO("=== 感知系统状态控制测试程序 ===");
        
        // 创建控制器
        StateController controller;
        
        // 初始化
        if(!controller.initialize()) {
            LOG_ERROR("初始化失败");
            LOG_INFO("请确保服务端程序(demo)已启动且有足够权限运行");
            return -1;
        }
        
        // 运行主循环（包含启动逻辑）
        controller.run();
        
        LOG_INFO("程序正常退出");
        return 0;
    }
    catch(const std::exception& e) {
        LOG_ERROR("错误: ", e.what());
        return -1;
    }
    catch(...) {
        LOG_ERROR("未知错误");
        return -1;
    }
} 