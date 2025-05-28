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
std::atomic<bool> g_exitRequested{false};
std::unique_ptr<ImageReceiver> g_imageReceiver;

/**
 * @brief 信号处理函数
 */
void signalHandler(int signal) {
    std::cout << "捕获信号: " << signal << std::endl;
    g_exitRequested = true;
    
    if (g_imageReceiver) {
        g_imageReceiver->stop();
    }
    
    // 使用更优雅的退出方式
    static std::atomic<bool> forceExitInProgress{false};
    
    if (!forceExitInProgress) {
        forceExitInProgress = true;
        std::thread([signal]() {
            // 等待一小段时间让程序正常退出
            std::this_thread::sleep_for(std::chrono::milliseconds(800));
            if (g_exitRequested) {
                std::cerr << "程序未能及时退出，强制退出" << std::endl;
                _exit(signal); // 使用 _exit 而不是 std::exit 避免析构函数阻塞
            }
        }).detach();
    }
}

int main() {
    try {
        // 设置信号处理
        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);

        std::cout << "=== Orbbec 相机启动程序 ===" << std::endl;
        std::cout << "正在启动..." << std::endl;
        
        // 获取配置并验证
        auto& config = ConfigHelper::getInstance();
        if (!config.validateAll()) {
            std::cerr << "配置验证失败!" << std::endl;
            return -1;
        }
        
        // 配置日志系统
        Logger::getInstance().setLevel(config.debugConfig.logLevel);
        if (!config.debugConfig.logFile.empty()) {
            Logger::getInstance().setLogFile(config.debugConfig.logFile);
        }
        
        // 打印当前配置
        config.printConfig();
        
        // 创建并初始化图像接收器
        g_imageReceiver = std::make_unique<ImageReceiver>();
        if (!g_imageReceiver->initialize()) {
            std::cerr << "无法初始化图像接收器" << std::endl;
            return -1;
        }
        
        std::cout << "相机初始化成功，开始启动..." << std::endl;
        
        // 启动图像接收器的管道
        if (!g_imageReceiver->setupPipelines()) {
            std::cerr << "无法设置图像管道" << std::endl;
            return -1;
        }
        
        if (!g_imageReceiver->startPipelines()) {
            std::cerr << "无法启动图像管道" << std::endl;
            return -1;
        }
        
        std::cout << "图像管道启动成功，开始运行..." << std::endl;
        
        // 运行主循环
        g_imageReceiver->run();
        
        // 如果正常退出，重置退出标志
        g_exitRequested = false;
        
        std::cout << "程序正常退出" << std::endl;
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "程序错误: " << e.what() << std::endl;
        return -1;
    }
    catch (...) {
        std::cerr << "发生未知错误" << std::endl;
        return -1;
    }
} 