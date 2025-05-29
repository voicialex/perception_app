#include <iostream>
#include <memory>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>
#include <filesystem>

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

/**
 * @brief 确保目录存在，如不存在则创建
 * @param dirPath 目录路径
 * @return 是否成功创建或已存在
 */
bool ensureDirectoryExists(const std::string& dirPath) {
    try {
        if (dirPath.empty()) {
            std::cerr << "目录路径为空!" << std::endl;
            return false;
        }
        
        // 检查目录是否存在，不存在则创建
        if (!std::filesystem::exists(dirPath)) {
            std::cout << "创建目录: " << dirPath << std::endl;
            return std::filesystem::create_directories(dirPath);
        }
        
        // 检查是否为目录
        if (!std::filesystem::is_directory(dirPath)) {
            std::cerr << "路径存在但不是目录: " << dirPath << std::endl;
            return false;
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "创建目录时出错: " << e.what() << std::endl;
        return false;
    }
}

int main() {
    try {
        // 设置信号处理
        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);

        std::cout << "=== Orbbec 相机启动程序 ===" << std::endl;
        std::cout << "正在启动..." << std::endl;
        
        // 设置日志级别为 DEBUG 以获取更多信息
        Logger::getInstance().setLevel("DEBUG");
        
        // 获取配置并修改关键参数
        auto& config = ConfigHelper::getInstance();
        
        // 修改配置以确保基本流打开并启用调试输出
        std::cout << "配置相机参数..." << std::endl;
        config.streamConfig.enableColor = true;
        config.streamConfig.enableDepth = true;
        config.renderConfig.enableRendering = true;
        config.renderConfig.showFPS = true;
        config.hotPlugConfig.enableHotPlug = true;
        config.hotPlugConfig.waitForDeviceOnStartup = true;
        config.hotPlugConfig.printDeviceEvents = true;
        config.debugConfig.enableDebugOutput = true;
        config.debugConfig.enablePerformanceStats = true;
        
        // 设置数据保存配置

        config.saveConfig.enableDump = true;
        config.saveConfig.dumpPath = "./dumps/";  // 使用当前目录下的dumps子目录
        config.saveConfig.saveColor = true;        // 保存彩色图像
        config.saveConfig.saveDepth = true;        // 保存深度图像
        config.saveConfig.imageFormat = "png";     // 使用PNG格式
        config.saveConfig.maxFramesToSave = 10000; // 最大保存1万帧
        
        // 确保保存路径存在
        if (!ensureDirectoryExists(config.saveConfig.dumpPath)) {
            std::cerr << "无法创建数据保存目录，将使用当前目录" << std::endl;
            config.saveConfig.dumpPath = "./";
        }
        
        if (!config.validateAll()) {
            std::cerr << "配置验证失败!" << std::endl;
            return -1;
        }
        
        // 打印当前配置
        config.printConfig();
        
        std::cout << "等待设备连接..." << std::endl;
        
        // 创建并初始化图像接收器
        g_imageReceiver = std::make_unique<ImageReceiver>();
        if (!g_imageReceiver->initialize()) {
            std::cerr << "无法初始化图像接收器" << std::endl;
            return -1;
        }
        
        std::cout << "相机初始化成功，开始运行..." << std::endl;
        
        // 使用新的简化接口直接启动流处理
        std::cout << "启动视频流处理..." << std::endl;
        if (!g_imageReceiver->startStreaming()) {
            std::cout << "启动视频流失败，将依赖热插拔机制，等待2秒..." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(2));
        } else {
            std::cout << "视频流启动成功!" << std::endl;
        }
        
        // 运行主循环
        std::cout << "启动主循环..." << std::endl;
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