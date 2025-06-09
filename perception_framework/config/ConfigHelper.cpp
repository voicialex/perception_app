#include "ConfigHelper.hpp"

// =================== 构造函数 ===================

ConfigHelper::ConfigHelper() {
    // 构造函数中可以进行额外的初始化
    if(!validateAll()) {
        LOG_WARN("Warning: Default configuration validation failed!");
    }
}

// =================== 配置验证实现 ===================

bool ConfigHelper::StreamConfig::validate() const {
    return (enableColor || enableDepth || enableIR || enableIRLeft || enableIRRight || enableIMU) &&
           colorWidth > 0 && colorHeight > 0 && colorFPS > 0 &&
           depthWidth > 0 && depthHeight > 0 && depthFPS > 0;
}

bool ConfigHelper::RenderConfig::validate() const {
    return windowWidth > 0 && windowHeight > 0 && !windowTitle.empty();
}

bool ConfigHelper::SaveConfig::validate() const {
    return !dumpPath.empty() && maxFramesToSave > 0 &&
           (imageFormat == "png" || imageFormat == "jpg" || imageFormat == "bmp") &&
           frameInterval > 0;
}

bool ConfigHelper::MetadataConfig::validate() const {
    // MetadataConfig 现在只包含显示格式选项，无需特殊验证
    return true;
}

bool ConfigHelper::HotPlugConfig::validate() const {
    return reconnectDelayMs >= 100 && maxReconnectAttempts > 0 && 
           deviceStabilizeDelayMs >= 0;
}

bool ConfigHelper::ParallelConfig::validate() const {
    return threadPoolSize >= 0 && maxQueuedTasks > 0;
}

bool ConfigHelper::InferenceConfig::validate() const {
    return inferenceInterval > 0 && defaultThreshold >= 0.0f && 
           defaultThreshold <= 1.0f && maxQueueSize > 0;
}

bool ConfigHelper::InferenceConfig::isValid() const {
    return validate(); // 兼容 InferenceManager 的命名
}

bool ConfigHelper::CalibrationConfig::validate() const {
    return boardWidth > 0 && boardHeight > 0 && squareSize > 0 && 
           minValidFrames > 0 && maxFrames >= minValidFrames && minInterval > 0;
}

bool ConfigHelper::LoggerConfig::validate() const {
    return (!enableFileLogging || !logDirectory.empty());
}

// =================== 日志系统实现 ===================

bool ConfigHelper::initializeLogger() {
    // 使用新的Logger高级接口，将所有目录管理交给Logger处理
    bool success = Logger::getInstance().initializeAdvanced(
        loggerConfig.logLevel,          // 日志级别
        loggerConfig.enableConsole,     // 控制台输出
        loggerConfig.enableFileLogging, // 文件日志
        loggerConfig.logDirectory,      // 日志目录
        "perception_app",               // 文件前缀
        true                           // 使用时间戳
    );
    
    if (success) {
        LOG_INFO("Configuration-based logger initialization completed");
    } else {
        LOG_ERROR("Failed to initialize logger with configuration");
    }
    
    return success;
}

void ConfigHelper::configureLogger(Logger::Level level, bool enableFile) {
    loggerConfig.logLevel = level;
    loggerConfig.enableFileLogging = enableFile;
    initializeLogger();
}

// =================== 配置管理实现 ===================

bool ConfigHelper::validateAll() const {
    return streamConfig.validate() && 
           renderConfig.validate() && 
           saveConfig.validate() && 
           metadataConfig.validate() && 
           hotPlugConfig.validate() && 
           parallelConfig.validate() &&
           inferenceConfig.validate() &&
           calibrationConfig.validate() &&
           loggerConfig.validate();
}

void ConfigHelper::printConfig() const {
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
             ", Metadata=", saveConfig.saveMetadata,
             ", MetadataConsole=", saveConfig.enableMetadataConsole,
             ", Interval=", saveConfig.frameInterval,
             ", FrameStats=", saveConfig.enableFrameStats);
    LOG_INFO("Metadata Format: ShowTimestamp=", metadataConfig.showTimestamp,
             ", ShowFrameNumber=", metadataConfig.showFrameNumber,
             ", ShowDeviceInfo=", metadataConfig.showDeviceInfo);
    LOG_INFO("HotPlug: Enabled=", hotPlugConfig.enableHotPlug, 
             ", AutoReconnect=", hotPlugConfig.autoReconnect, 
             ", MaxAttempts=", hotPlugConfig.maxReconnectAttempts);
    LOG_INFO("Parallel: Enabled=", parallelConfig.enableParallelProcessing, 
             ", ThreadPoolSize=", parallelConfig.threadPoolSize, 
             ", MaxQueuedTasks=", parallelConfig.maxQueuedTasks);
    LOG_INFO("Inference: Enabled=", inferenceConfig.enableInference,
             ", DefaultModel=", inferenceConfig.defaultModel,
             ", DefaultModelType=", inferenceConfig.defaultModelType,
             ", DefaultThreshold=", inferenceConfig.defaultThreshold,
             ", PerformanceStats=", inferenceConfig.enablePerformanceStats);
    LOG_INFO("Calibration: Enabled=", calibrationConfig.enableCalibration,
             ", BoardWidth=", calibrationConfig.boardWidth,
             ", BoardHeight=", calibrationConfig.boardHeight,
             ", SquareSize=", calibrationConfig.squareSize,
             ", MinValidFrames=", calibrationConfig.minValidFrames,
             ", MaxFrames=", calibrationConfig.maxFrames,
             ", MinInterval=", calibrationConfig.minInterval);
    LOG_INFO("Logger: Level=", static_cast<int>(loggerConfig.logLevel),
             ", FileLogging=", loggerConfig.enableFileLogging ? "enabled" : "disabled");
    LOG_INFO("============================");
}

void ConfigHelper::resetToDefaults() {
    streamConfig = StreamConfig{};
    renderConfig = RenderConfig{};
    saveConfig = SaveConfig{};
    metadataConfig = MetadataConfig{};
    hotPlugConfig = HotPlugConfig{};
    parallelConfig = ParallelConfig{};
    inferenceConfig = InferenceConfig{};
    calibrationConfig = CalibrationConfig{};
    loggerConfig = LoggerConfig{};
} 