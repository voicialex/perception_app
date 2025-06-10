#include <iostream>
#include <memory>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>
#include <filesystem>
#include <unistd.h>

#include "core/PerceptionSystem.hpp"
#include "config/ConfigHelper.hpp"
#include "utils/Logger.hpp"

// 全局变量用于信号处理
std::atomic<bool> g_exitRequested{false};

/**
 * @brief SIGALRM 信号处理函数 - 强制退出
 */
void alarmHandler(int signal) {
    (void)signal;
    std::_Exit(1);  // 立即强制退出
}

/**
 * @brief 主信号处理函数 - 设置超时后优雅退出
 */
void signalHandler(int signal) {
    static std::atomic<bool> firstSignal{true};
    
    // 避免未使用参数警告
    (void)signal;
    
    if (firstSignal.exchange(false)) {
        // 第一次收到信号，设置优雅退出
        g_exitRequested = true;
        
        // 停止感知系统
        PerceptionSystem::getInstance().stop();
        
        // 设置 5 秒超时，如果无法正常退出则强制退出
        std::signal(SIGALRM, alarmHandler);
        alarm(5);  // 5 秒后发送 SIGALRM 信号强制退出
    } else {
        // 再次收到信号，立即强制退出
        std::_Exit(1);
    }
}

int main(int argc, char** argv) {
    int returnCode = 0;
    bool skipCommunication = true;
    
    // 检查是否有跳过通信的参数
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "--skip-comm") {
            skipCommunication = true;
            break;
        }
    }
    
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
        config.renderConfig.enableRendering = true;  // 启用窗口渲染
        config.renderConfig.showFPS = true;
        config.hotPlugConfig.enableHotPlug = true;
        config.hotPlugConfig.waitForDeviceOnStartup = true;
        config.hotPlugConfig.printDeviceEvents = true;
        config.inferenceConfig.enablePerformanceStats = true;
        config.saveConfig.enableFrameStats = true;
        config.saveConfig.enableMetadataConsole = false;  // 禁用元数据控制台显示
        
        // 跳过通信代理 - 避免阻塞
        if (skipCommunication) {
            config.communicationConfig.enableCommunication = false;
            LOG_INFO("Communication proxy disabled by command line option");
        }
        
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
        
        LOG_INFO("Initializing perception system...");
        
        // 获取感知系统单例并初始化
        auto& system = PerceptionSystem::getInstance();
        if (!system.initialize()) {
            LOG_ERROR("Cannot initialize perception system");
            return -1;
        }
        
        LOG_INFO("Starting perception system...");
        
        // 如果不需要通信代理，直接进入图像接收逻辑
        if (skipCommunication) {
            // 创建一个独立的 ImageReceiver 用于显示
            auto imageReceiver = std::make_shared<ImageReceiver>();
            if (!imageReceiver->initialize()) {
                LOG_ERROR("Cannot initialize image receiver");
                return -1;
            }
            
            LOG_INFO("Starting video stream processing...");
            if (!imageReceiver->startStreaming()) {
                LOG_INFO("Failed to start video stream, will retry...");
                std::this_thread::sleep_for(std::chrono::seconds(2));
                if (!imageReceiver->startStreaming()) {
                    LOG_ERROR("Failed to start video stream after retry");
                    return -1;
                }
            }
            
            // 在子线程中运行数据处理
            std::thread processingThread([&imageReceiver]() {
                imageReceiver->run();
            });
            processingThread.detach();
            
            // 主线程处理窗口显示
            LOG_INFO("Starting main display loop...");
            
            // 主线程处理窗口事件和显示
            while (!g_exitRequested) {
                auto window = imageReceiver->getWindow();
                if (window) {
                    if (!window->processEvents()) {
                        LOG_INFO("Window closed");
                        break;
                    }
                    window->updateWindow();
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        } else {
            // 启动感知系统
            LOG_INFO("Starting video stream processing...");
            LOG_INFO("Press Ctrl+C to exit (program will force quit after 5 seconds if needed)");
            
            // 运行主循环（阻塞，直到窗口关闭或系统停止）
            system.run();
        }
        
        // 优雅退出
        LOG_INFO("Exit signal received, shutting down gracefully...");
        
        // 停止感知系统
        system.stop();
        
        // 取消闹钟（如果程序正常退出）
        alarm(0);
        
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
    
    return returnCode;
} 