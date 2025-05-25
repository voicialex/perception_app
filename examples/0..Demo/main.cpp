// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include <iostream>
#include <memory>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>
#include "ImageReceiver.hpp"
#include "ConfigHelper.hpp"
#include "Logger.hpp"

// 全局变量用于信号处理
std::unique_ptr<ImageReceiver> g_receiver;
std::atomic<bool> g_exitRequested{false};

/**
 * @brief 信号处理函数
 * @param signal 信号类型
 */
void signalHandler(int signal) {
    LOG_INFO("Received signal ", signal, ", shutting down gracefully...");
    g_exitRequested = true;
    
    if(g_receiver) {
        g_receiver->stop();
    }
    
    // 启动强制退出定时器
    std::thread([]() {
        std::this_thread::sleep_for(std::chrono::seconds(3));
        if(g_exitRequested) {
            LOG_WARN("Force exit after 3 seconds timeout");
            std::exit(0);
        }
    }).detach();
}

/**
 * @brief 主函数 - 简化版本，无需手动配置
 */
int main() {
    try {
        // 设置信号处理
        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);

        LOG_INFO("=== Orbbec Camera Demo with Hot Plug Support ===");
        LOG_INFO("Starting application...");
        
        // 获取配置并验证
        auto& config = ConfigHelper::getInstance();
        if(!config.validateAll()) {
            LOG_ERROR("Configuration validation failed!");
            return -1;
        }
        
        // 设置日志系统
        Logger::getInstance().setLevel(config.debugConfig.logLevel);
        if(!config.debugConfig.logFile.empty()) {
            Logger::getInstance().setLogFile(config.debugConfig.logFile);
        }
        
        // 打印当前配置
        if(config.debugConfig.enableDebugOutput) {
            config.printConfig();
        }
        
        // 创建图像接收器
        g_receiver = std::make_unique<ImageReceiver>();
        
        // 初始化
        if(!g_receiver->initialize()) {
            LOG_ERROR("Failed to initialize ImageReceiver");
            return -1;
        }
        
        LOG_INFO("Application initialized successfully");
        LOG_INFO("Controls: ESC-Exit, R-Reboot, P-Print devices, Ctrl+C-Exit");
        
        // 运行主循环
        g_receiver->run();
        
        // 清理全局变量
        g_receiver.reset();
        g_exitRequested = false;
        
        LOG_INFO("Application exiting normally");
        return 0;
    }
    catch(const std::exception& e) {
        LOG_ERROR("Application error: ", e.what());
        g_receiver.reset();
        return -1;
    }
    catch(...) {
        LOG_ERROR("Unknown error occurred");
        g_receiver.reset();
        return -1;
    }
}
