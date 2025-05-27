#pragma once

#include <string>
#include <iostream>
#include <fstream>
#include <map>

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
        bool enableIR = false;               // 启用红外流
        bool enableIRLeft = false;           // 启用左红外流
        bool enableIRRight = false;          // 启用右红外流
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
        bool savePointCloud = false;        // 保存点云数据
        std::string imageFormat = "png";     // 图像格式
        int maxFramesToSave = 1000;          // 最大保存帧数
        
        bool validate() const {
            return !dumpPath.empty() && maxFramesToSave > 0 &&
                   (imageFormat == "png" || imageFormat == "jpg" || imageFormat == "bmp");
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
               debugConfig.validate();
    }

    /**
     * @brief 打印当前配置
     */
    void printConfig() const {
        std::cout << "\n=== Current Configuration ===" << std::endl;
        std::cout << "Stream: Color=" << streamConfig.enableColor 
                  << ", Depth=" << streamConfig.enableDepth 
                  << ", IMU=" << streamConfig.enableIMU << std::endl;
        std::cout << "Render: " << renderConfig.windowWidth << "x" << renderConfig.windowHeight 
                  << ", Title=" << renderConfig.windowTitle << std::endl;
        std::cout << "HotPlug: Enabled=" << hotPlugConfig.enableHotPlug 
                  << ", AutoReconnect=" << hotPlugConfig.autoReconnect 
                  << ", MaxAttempts=" << hotPlugConfig.maxReconnectAttempts << std::endl;
        std::cout << "Debug: Level=" << debugConfig.logLevel 
                  << ", Performance=" << debugConfig.enablePerformanceStats << std::endl;
        std::cout << "============================\n" << std::endl;
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
    }

private:
    ConfigHelper() {
        // 构造函数中可以进行额外的初始化
        if(!validateAll()) {
            std::cerr << "Warning: Default configuration validation failed!" << std::endl;
        }
    }
    
    ~ConfigHelper() = default;
    ConfigHelper(const ConfigHelper&) = delete;
    ConfigHelper& operator=(const ConfigHelper&) = delete;
}; 