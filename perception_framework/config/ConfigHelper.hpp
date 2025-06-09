#pragma once

#include <string>
#include <iostream>
#include <fstream>
#include <map>
#include <filesystem>
#include <chrono>
#include <sstream>
#include <iomanip>
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
        bool enableIR = true;                // 启用红外流
        bool enableIRLeft = true;            // 启用左红外流
        bool enableIRRight = true;           // 启用右红外流
        bool enableIMU = false;              // 启用IMU数据流
        
        // 流质量配置
        int colorWidth = 1280;               // 彩色流宽度
        int colorHeight = 720;               // 彩色流高度
        int colorFPS = 30;                   // 彩色流帧率
        int depthWidth = 1280;               // 深度流宽度
        int depthHeight = 720;               // 深度流高度
        int depthFPS = 30;                   // 深度流帧率
        
        bool validate() const;
    } streamConfig;

    // 渲染配置
    struct RenderConfig {
        bool enableRendering = true;         // 启用渲染
        int windowWidth = 1280;              // 窗口宽度
        int windowHeight = 720;              // 窗口高度
        bool showFPS = true;                 // 显示FPS
        bool autoResize = true;              // 自动调整窗口大小
        std::string windowTitle = "Orbbec Camera Demo";  // 窗口标题
        
        bool validate() const;
    } renderConfig;

    // 数据保存配置
    struct SaveConfig {
        bool enableDump = false;             // 启用数据保存
        std::string dumpPath = "./dumps/";   // 保存路径
        bool saveColor = true;               // 保存彩色图像
        bool saveDepth = true;               // 保存深度图像
        bool saveDepthColormap = true;       // 保存深度图像的colormap版本
        bool saveDepthData = true;           // 保存深度数据的纯数字格式(CSV)
        bool saveIR = true;                  // 保存红外图像
        bool savePointCloud = false;         // 保存点云数据
        std::string imageFormat = "png";     // 图像格式
        int maxFramesToSave = 1000;          // 最大保存帧数
        int frameInterval = 500;             // 保存帧间隔（每N帧保存一帧，值越大保存频率越低）
        bool enableFrameStats = false;       // 启用帧统计信息
        
        bool validate() const;
    } saveConfig;

    // 元数据显示配置
    struct MetadataConfig {
        bool enableMetadata = false;         // 启用元数据显示（默认关闭减少输出）
        int printInterval = 300;             // 打印间隔(帧数)
        bool showTimestamp = true;           // 显示时间戳
        bool showFrameNumber = true;         // 显示帧号
        bool showDeviceInfo = true;          // 显示设备信息
        bool enableTimingInfo = false;       // 启用详细时序信息
        int statsInterval = 30;              // 统计信息输出间隔(秒)
        
        bool validate() const;
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
        
        bool validate() const;
    } hotPlugConfig;

    // 并行处理配置
    struct ParallelConfig {
        bool enableParallelProcessing = true;    // 启用并行处理
        int threadPoolSize = 4;                  // 线程池大小（0表示使用硬件并发数）
        int maxQueuedTasks = 100;                // 最大排队任务数
        
        bool validate() const;
    } parallelConfig;

    // 推理配置
    struct InferenceConfig {
        bool enableInference = false;              // 是否启用推理
        std::string defaultModel = "";             // 默认模型路径
        std::string defaultModelType = "";         // 默认模型类型
        float defaultThreshold = 0.5f;             // 默认置信度阈值
        bool enableVisualization = true;           // 是否启用可视化
        bool enablePerformanceStats = false;       // 是否启用性能统计
        int inferenceInterval = 1;                 // 推理间隔（每N帧推理一次）
        std::string classNamesFile = "";           // 类别名称文件路径
        bool asyncInference = true;                // 是否异步推理
        int maxQueueSize = 10;                     // 异步推理队列最大大小
        std::string modelsDirectory = "./models/"; // 模型目录
        bool enableFramePreprocessing = true;      // 是否启用帧预处理
        bool onlyProcessColorFrames = true;        // 是否只处理彩色帧
        
        bool validate() const;
        bool isValid() const; // 兼容 InferenceManager 的命名
    } inferenceConfig;

    // 相机标定配置
    struct CalibrationConfig {
        bool enableCalibration = false;               // 是否启用标定功能
        int boardWidth = 9;                           // 棋盘宽度（内角点数）
        int boardHeight = 6;                          // 棋盘高度（内角点数）
        float squareSize = 1.0f;                      // 方格大小（实际物理尺寸，单位：mm）
        int minValidFrames = 20;                      // 最少需要的有效帧数
        int maxFrames = 50;                           // 最大采集帧数
        double minInterval = 1.0;                     // 采集间隔（秒）
        bool useSubPixel = true;                      // 是否使用亚像素精度
        bool enableUndistortion = true;               // 是否启用去畸变
        std::string saveDirectory = "./calibration/"; // 保存目录
        bool autoStartCalibrationOnStartup = false;   // 启动时自动开始标定
        bool showCalibrationProgress = true;          // 显示标定进度
        
        bool validate() const;
    } calibrationConfig;

    // 日志系统配置 - 简化版本
    struct LoggerConfig {
        Logger::Level logLevel = Logger::Level::INFO;   // 日志级别
        bool enableConsole = true;                      // 是否启用控制台输出
        bool enableFileLogging = true;                  // 是否启用文件日志
        std::string logDirectory = "logs/";             // 日志目录
        
        bool validate() const;
    } loggerConfig;

    /**
     * @brief 初始化日志系统
     */
    bool initializeLogger();

    /**
     * @brief 快速配置日志系统
     * @param level 日志级别
     * @param enableFile 是否启用文件日志  
     */
    void configureLogger(Logger::Level level = Logger::Level::INFO, 
                        bool enableFile = true);

    /**
     * @brief 验证所有配置的有效性
     * @return true if all configurations are valid
     */
    bool validateAll() const;

    /**
     * @brief 打印当前配置
     */
    void printConfig() const;

    /**
     * @brief 重置为默认配置
     */
    void resetToDefaults();

private:
    ConfigHelper();
    ~ConfigHelper() = default;
    ConfigHelper(const ConfigHelper&) = delete;
    ConfigHelper& operator=(const ConfigHelper&) = delete;
}; 