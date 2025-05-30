#pragma once

#include <string>
#include <iostream>
#include <fstream>
#include <map>
#include <filesystem>
#include "Logger.hpp"

/**
 * @brief 配置管理器 - 单例模式
 * 提供应用程序的所有配置选项，支持配置验证和持久化
 */
class ConfigHelper {
public:
    static ConfigHelper& getInstance() {
        static ConfigHelper instance;
        return instance;
    }

    // 数据流配置
    struct StreamConfig {
        bool enableColor = true;             // 启用彩色流
        bool enableDepth = true;             // 启用深度流
        bool enableIR = true;               // 启用红外流
        bool enableIRLeft = true;           // 启用左红外流
        bool enableIRRight = true;          // 启用右红外流
        bool enableIMU = false;              // 启用IMU数据流
        
        // 流质量配置
        int colorWidth = 1280;               // 彩色流宽度
        int colorHeight = 720;               // 彩色流高度
        int colorFPS = 30;                   // 彩色流帧率
        int depthWidth = 1280;               // 深度流宽度
        int depthHeight = 720;               // 深度流高度
        int depthFPS = 30;                   // 深度流帧率
        
        bool validate() const {
            return (enableColor || enableDepth || enableIR || enableIRLeft || enableIRRight || enableIMU) &&
                   colorWidth > 0 && colorHeight > 0 && colorFPS > 0 &&
                   depthWidth > 0 && depthHeight > 0 && depthFPS > 0;
        }
    } streamConfig;

    // 渲染配置
    struct RenderConfig {
        bool enableRendering = true;         // 启用渲染
        int windowWidth = 1280;              // 窗口宽度
        int windowHeight = 720;              // 窗口高度
        bool showFPS = true;                 // 显示FPS
        bool autoResize = true;              // 自动调整窗口大小
        std::string windowTitle = "Orbbec Camera Demo";  // 窗口标题
        
        bool validate() const {
            return windowWidth > 0 && windowHeight > 0 && !windowTitle.empty();
        }
    } renderConfig;

    // 数据保存配置
    struct SaveConfig {
        bool enableDump = false;             // 启用数据保存
        std::string dumpPath = "./dumps/";   // 保存路径
        bool saveColor = true;               // 保存彩色图像
        bool saveDepth = true;               // 保存深度图像
        bool saveIR = true;                  // 保存红外图像
        bool savePointCloud = false;         // 保存点云数据
        std::string imageFormat = "png";     // 图像格式
        int maxFramesToSave = 1000;          // 最大保存帧数
        int frameInterval = 100;              // 保存帧间隔（每N帧保存一帧，值越大保存频率越低）
        
        bool validate() const {
            return !dumpPath.empty() && maxFramesToSave > 0 &&
                   (imageFormat == "png" || imageFormat == "jpg" || imageFormat == "bmp") &&
                   frameInterval > 0;
        }
    } saveConfig;

    // 元数据显示配置
    struct MetadataConfig {
        bool enableMetadata = false;         // 启用元数据显示（默认关闭减少输出）
        int printInterval = 300;             // 打印间隔(帧数)
        bool showTimestamp = true;           // 显示时间戳
        bool showFrameNumber = true;         // 显示帧号
        bool showDeviceInfo = true;          // 显示设备信息
        
        bool validate() const {
            return printInterval > 0;
        }
    } metadataConfig;

    // 热插拔配置
    struct HotPlugConfig {
        bool enableHotPlug = true;           // 启用热插拔功能
        bool autoReconnect = true;           // 自动重连设备
        bool printDeviceEvents = true;       // 打印设备事件信息
        int reconnectDelayMs = 1000;         // 重连延迟时间(毫秒)
        int maxReconnectAttempts = 30;       // 最大重连尝试次数
        int deviceStabilizeDelayMs = 500;    // 设备稳定等待时间
        bool waitForDeviceOnStartup = true;  // 启动时等待设备连接
        
        bool validate() const {
            return reconnectDelayMs >= 100 && maxReconnectAttempts > 0 && 
                   deviceStabilizeDelayMs >= 0;
        }
    } hotPlugConfig;

    // 调试配置
    struct DebugConfig {
        bool enableDebugOutput = false;      // 启用调试输出
        bool enablePerformanceStats = false; // 启用性能统计
        bool enableErrorLogging = true;      // 启用错误日志
        std::string logLevel = "INFO";       // 日志级别: DEBUG, INFO, WARN, ERROR
        std::string logFile = "";            // 日志文件路径（空表示控制台输出）
        
        bool validate() const {
            return logLevel == "DEBUG" || logLevel == "INFO" || 
                   logLevel == "WARN" || logLevel == "ERROR";
        }
    } debugConfig;

    // 并行处理配置
    struct ParallelConfig {
        bool enableParallelProcessing = true;    // 启用并行处理
        int threadPoolSize = 4;                  // 线程池大小（0表示使用硬件并发数）
        int maxQueuedTasks = 100;                // 最大排队任务数
        
        bool validate() const {
            return threadPoolSize >= 0 && maxQueuedTasks > 0;
        }
    } parallelConfig;

    /**
     * @brief 确保目录存在，并规范化路径
     * @param path 需要检查和创建的目录路径
     * @param addTrailingSlash 是否确保路径末尾有斜杠
     * @return 规范化后的路径，如果创建失败则返回空字符串
     */
    static std::string ensureDirectoryExists(const std::string& path, bool addTrailingSlash = true) {
        try {
            if(path.empty()) {
                LOG_ERROR("Path is empty");
                return "";
            }
            
            // 规范化路径
            std::string normPath = path;
            
            // 确保路径末尾有斜杠（如果需要）
            if(addTrailingSlash && !normPath.empty() && normPath.back() != '/') {
                normPath += '/';
            }
            
            // 创建目录（如果不存在）
            if(!std::filesystem::exists(normPath)) {
                if(std::filesystem::create_directories(normPath)) {
                    LOG_INFO("Directory created: ", normPath);
                } else {
                    LOG_ERROR("Failed to create directory: ", normPath);
                    return "";
                }
            }
            
            return normPath;
        }
        catch(const std::exception& e) {
            LOG_ERROR("Error processing directory: ", e.what());
            return "";
        }
    }

    /**
     * @brief 确保保存目录存在
     * @return 如果目录存在或创建成功则返回true
     */
    bool ensureSaveDirectoryExists() {
        std::string normPath = ensureDirectoryExists(saveConfig.dumpPath);
        if(!normPath.empty()) {
            saveConfig.dumpPath = normPath;
            return true;
        }
        return false;
    }

    /**
     * @brief 验证所有配置的有效性
     * @return true if all configurations are valid
     */
    bool validateAll() const {
        return streamConfig.validate() && 
               renderConfig.validate() && 
               saveConfig.validate() && 
               metadataConfig.validate() && 
               hotPlugConfig.validate() && 
               debugConfig.validate() &&
               parallelConfig.validate();
    }

    /**
     * @brief 打印当前配置
     */
    void printConfig() const {
        LOG_INFO("=== Current Configuration ===");
        LOG_INFO("Stream: Color=", streamConfig.enableColor, 
                 ", Depth=", streamConfig.enableDepth, 
                 ", IR=", streamConfig.enableIR,
                 ", IR_Left=", streamConfig.enableIRLeft,
                 ", IR_Right=", streamConfig.enableIRRight,
                 ", IMU=", streamConfig.enableIMU);
        LOG_INFO("Render: ", renderConfig.windowWidth, "x", renderConfig.windowHeight, 
                 ", Title=", renderConfig.windowTitle);
        LOG_INFO("Save: Enabled=", saveConfig.enableDump,
                 ", Color=", saveConfig.saveColor,
                 ", Depth=", saveConfig.saveDepth,
                 ", IR=", saveConfig.saveIR,
                 ", Interval=", saveConfig.frameInterval);
        LOG_INFO("HotPlug: Enabled=", hotPlugConfig.enableHotPlug, 
                 ", AutoReconnect=", hotPlugConfig.autoReconnect, 
                 ", MaxAttempts=", hotPlugConfig.maxReconnectAttempts);
        LOG_INFO("Debug: Level=", debugConfig.logLevel, 
                 ", Performance=", debugConfig.enablePerformanceStats);
        LOG_INFO("Parallel: Enabled=", parallelConfig.enableParallelProcessing, 
                 ", ThreadPoolSize=", parallelConfig.threadPoolSize, 
                 ", MaxQueuedTasks=", parallelConfig.maxQueuedTasks);
        LOG_INFO("============================");
    }

    /**
     * @brief 重置为默认配置
     */
    void resetToDefaults() {
        streamConfig = StreamConfig{};
        renderConfig = RenderConfig{};
        saveConfig = SaveConfig{};
        metadataConfig = MetadataConfig{};
        hotPlugConfig = HotPlugConfig{};
        debugConfig = DebugConfig{};
        parallelConfig = ParallelConfig{};
    }

private:
    ConfigHelper() {
        // 构造函数中可以进行额外的初始化
        if(!validateAll()) {
            LOG_WARN("Warning: Default configuration validation failed!");
        }
    }
    
    ~ConfigHelper() = default;
    ConfigHelper(const ConfigHelper&) = delete;
    ConfigHelper& operator=(const ConfigHelper&) = delete;
}; 