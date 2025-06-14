/**
 * @file main.cpp
 * @brief 感知系统主程序入口
 * @details 负责整个感知系统的启动、配置和退出处理
 * @version 2.4.3
 */

// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include <iostream>
#include <memory>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>
#include "ConfigHelper.hpp"
#include "core/PerceptionSystem.hpp"
#include "Logger.hpp"
#include "CommunicationProxy.hpp"

// 全局变量只用于信号处理
std::atomic<bool> g_exitRequested{false};

/**
 * @brief 信号处理函数
 */
void signalHandler(int signal) {
    LOG_INFO("Caught signal: ", signal);
    g_exitRequested = true;
    
    // 使用单例获取PerceptionSystem
    PerceptionSystem::getInstance().stop();
    
    // 使用更优雅的退出方式
    static std::atomic<bool> forceExitInProgress{false};
    
    if(!forceExitInProgress) {
        forceExitInProgress = true;
        std::thread([signal]() {
            // 等待一小段时间让程序正常退出
            std::this_thread::sleep_for(std::chrono::milliseconds(800));
            if(g_exitRequested) {
                LOG_WARN("Program did not exit in time, forcing exit");
                _exit(signal); // 使用 _exit 而不是 std::exit 避免析构函数阻塞
            }
        }).detach();
    }
}

/**
 * @brief 主函数 - 使用感知系统
 */
int main() {
    try {
        // 设置信号处理
        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);

        LOG_INFO("=== Orbbec Camera Perception System ===");
        LOG_INFO("Starting application...");
        
        // =================== 获取配置并设置日志系统 ===================
        
        // 获取配置实例
        auto& config = ConfigHelper::getInstance();

        // 简化的日志配置
        config.configureLogger(Logger::Level::DEBUG, true);  // DEBUG级别，启用文件日志
        
        // =================== 继续原有逻辑 ===================
        
        // 添加这里：明确设置关键配置参数
        config.streamConfig.enableColor = true;
        config.streamConfig.enableDepth = true;
        config.renderConfig.enableRendering = true;
        config.hotPlugConfig.enableHotPlug = true;
        config.hotPlugConfig.waitForDeviceOnStartup = true;
        config.inferenceConfig.enablePerformanceStats = true;

        if(!config.validateAll()) {
            LOG_ERROR("Configuration validation failed!");
            return -1;
        }
        
        // 打印当前配置
            config.printConfig();
        
        // 初始化通信代理 - 作为服务端
        auto& commProxy = CommunicationProxy::getInstance();
        if(!commProxy.initialize("/tmp/orbbec_camera", CommunicationProxy::CommRole::SERVER)) {
            LOG_ERROR("Failed to initialize CommunicationProxy");
            return -1;
        }
        
        // 获取感知系统实例
        auto& perceptionSystem = PerceptionSystem::getInstance();
        
        // 初始化
        LOG_ERROR("perceptionSystem.initialize() 1");
        if(!perceptionSystem.initialize()) {
            LOG_ERROR("Failed to initialize PerceptionSystem");
            return -1;
        }
        LOG_ERROR("perceptionSystem.initialize() 2");
        
        // 运行主循环（现在包含了启动逻辑）
        perceptionSystem.run();
        
        // 重置退出标志
        g_exitRequested = false;
        
        LOG_INFO("Application exiting normally");
        return 0;
    }
    catch(const std::exception& e) {
        LOG_ERROR("Application error: ", e.what());
        return -1;
    }
    catch(...) {
        LOG_ERROR("Unknown error occurred");
        return -1;
    }
}
