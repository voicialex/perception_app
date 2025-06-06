#include <iostream>
#include <memory>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>
#include <filesystem>

#include "core/ImageReceiver.hpp"
#include "config/ConfigHelper.hpp"
#include "utils/Logger.hpp"

// 全局变量用于信号处理
std::atomic<bool> g_exitRequested{false};
std::unique_ptr<ImageReceiver> g_imageReceiver;

/**
 * @brief 信号处理函数 - 只使用异步信号安全的操作
 */
void signalHandler(int signal) {
    // 避免未使用参数警告
    (void)signal;
    
    // 只使用异步信号安全的操作
    g_exitRequested = true;
    
    if (g_imageReceiver) {
        g_imageReceiver->stop();
    }
    
    // 不在信号处理函数中使用日志或创建线程
    // 让主线程处理退出逻辑
}

int main() {
    int returnCode = 0;
    
    try {
        // 获取配置实例并首先配置日志系统
        auto& config = ConfigHelper::getInstance();
        
        // 配置日志系统 - camera_bin 使用文件日志
        config.loggerConfig.logLevel = Logger::Level::INFO;
        config.loggerConfig.logDirectory = "./logs/";
        config.loggerConfig.enableFileLogging = true;  // camera_bin 启用文件日志
        config.loggerConfig.enableConsole = true;
        
        // 通过 ConfigHelper 统一初始化日志系统
        if (!config.initializeLogger()) {
            std::cerr << "Failed to initialize logger through ConfigHelper" << std::endl;
            return -1;
        }
        
        LOG_INFO("=== Orbbec Camera Launcher ===");
        LOG_INFO("Starting up...");
        
        // 设置信号处理
        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);
        
        LOG_INFO("Configuring camera parameters...");
        
        // 配置相机参数
        config.streamConfig.enableColor = true;
        config.streamConfig.enableDepth = true;
        config.streamConfig.enableIR = true;
        config.metadataConfig.enableMetadata = true;
        config.renderConfig.enableRendering = false;  // 无头模式
        config.renderConfig.showFPS = true;
        config.hotPlugConfig.enableHotPlug = true;
        config.hotPlugConfig.waitForDeviceOnStartup = true;
        config.hotPlugConfig.printDeviceEvents = true;
        config.inferenceConfig.enablePerformanceStats = true;
        config.saveConfig.enableFrameStats = true;
        
        // 配置数据保存
        config.saveConfig.enableDump = true;
        config.saveConfig.dumpPath = "./dumps/";
        config.saveConfig.saveColor = true;
        config.saveConfig.saveDepth = true;
        config.saveConfig.saveDepthColormap = true;
        config.saveConfig.saveIR = true;
        config.saveConfig.imageFormat = "png";
        config.saveConfig.maxFramesToSave = 1000;
        
        // 验证配置
        if (!config.validateAll()) {
            LOG_ERROR("Configuration validation failed!");
            return -1;
        }
        
        // 打印配置
        config.printConfig();
        
        LOG_INFO("Waiting for device connection...");
        
        // 创建并初始化图像接收器
        g_imageReceiver = std::make_unique<ImageReceiver>();
        if (!g_imageReceiver->initialize()) {
            LOG_ERROR("Cannot initialize image receiver");
            return -1;
        }
        
        LOG_INFO("Camera initialized successfully, starting operation...");
        
        // 启动视频流处理
        LOG_INFO("Starting video stream processing...");
        if (!g_imageReceiver->startStreaming()) {
            LOG_INFO("Failed to start video stream, will rely on hot-plug mechanism, waiting 2 seconds...");
            std::this_thread::sleep_for(std::chrono::seconds(2));
        } else {
            LOG_INFO("Video stream started successfully!");
        }
        
        // 主循环
        LOG_INFO("Starting main loop...");
        
        // 在单独的线程中运行图像接收器
        std::thread receiverThread([&]() {
            g_imageReceiver->run();
        });
        
        // 主线程监听退出信号
        while (!g_exitRequested) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        // 优雅退出
        LOG_INFO("Exit signal received, shutting down gracefully...");
        
        if (g_imageReceiver) {
            g_imageReceiver->stop();
        }
        
        // 等待接收器线程结束
        if (receiverThread.joinable()) {
            receiverThread.join();
        }
        
        LOG_INFO("Program exited normally");
        
    } catch (const std::exception& e) {
        if (Logger::getInstance().isInitialized()) {
            LOG_ERROR("Program error: {}", e.what());
        } else {
            std::cerr << "Program error: " << e.what() << std::endl;
        }
        returnCode = -1;
    } catch (...) {
        if (Logger::getInstance().isInitialized()) {
            LOG_ERROR("Unknown error occurred");
        } else {
            std::cerr << "Unknown error occurred" << std::endl;
        }
        returnCode = -1;
    }
    
    // 清理资源
    g_imageReceiver.reset();
    
    return returnCode;
} 