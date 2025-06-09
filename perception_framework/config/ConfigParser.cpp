#include "ConfigParser.hpp"
#include <fstream>
#include <iostream>

// =================== 公共接口实现 ===================

bool ConfigParser::loadFromFile(const std::string& filepath) {
    try {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            std::cerr << "Failed to open config file: " << filepath << std::endl;
            return false;
        }
        
        Json::CharReaderBuilder builder;
        Json::Value root;
        std::string errs;
        
        if (!Json::parseFromStream(builder, file, &root, &errs)) {
            std::cerr << "Failed to parse JSON: " << errs << std::endl;
            return false;
        }
        
        auto& configHelper = ConfigHelper::getInstance();
        
        // 解析各个配置段
        if (root.isMember("stream")) {
            parseStreamConfig(root["stream"], configHelper.streamConfig);
        }
        if (root.isMember("render")) {
            parseRenderConfig(root["render"], configHelper.renderConfig);
        }
        if (root.isMember("save")) {
            parseSaveConfig(root["save"], configHelper.saveConfig);
        }
        if (root.isMember("metadata")) {
            parseMetadataConfig(root["metadata"], configHelper.metadataConfig);
        }
        if (root.isMember("hotplug")) {
            parseHotPlugConfig(root["hotplug"], configHelper.hotPlugConfig);
        }
        if (root.isMember("parallel")) {
            parseParallelConfig(root["parallel"], configHelper.parallelConfig);
        }
        if (root.isMember("inference")) {
            parseInferenceConfig(root["inference"], configHelper.inferenceConfig);
        }
        if (root.isMember("calibration")) {
            parseCalibrationConfig(root["calibration"], configHelper.calibrationConfig);
        }
        if (root.isMember("logger")) {
            parseLoggerConfig(root["logger"], configHelper.loggerConfig);
        }
        
        std::cout << "Configuration loaded successfully from: " << filepath << std::endl;
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Exception while loading config: " << e.what() << std::endl;
        return false;
    }
}

bool ConfigParser::saveToFile(const std::string& filepath) {
    try {
        auto& configHelper = ConfigHelper::getInstance();
        
        Json::Value root;
        root["stream"] = streamConfigToJson(configHelper.streamConfig);
        root["render"] = renderConfigToJson(configHelper.renderConfig);
        root["save"] = saveConfigToJson(configHelper.saveConfig);
        root["metadata"] = metadataConfigToJson(configHelper.metadataConfig);
        root["hotplug"] = hotPlugConfigToJson(configHelper.hotPlugConfig);
        root["parallel"] = parallelConfigToJson(configHelper.parallelConfig);
        root["inference"] = inferenceConfigToJson(configHelper.inferenceConfig);
        root["calibration"] = calibrationConfigToJson(configHelper.calibrationConfig);
        root["logger"] = loggerConfigToJson(configHelper.loggerConfig);
        
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "  ";
        std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
        
        std::ofstream file(filepath);
        if (!file.is_open()) {
            std::cerr << "Failed to open file for writing: " << filepath << std::endl;
            return false;
        }
        
        writer->write(root, &file);
        std::cout << "Configuration saved successfully to: " << filepath << std::endl;
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Exception while saving config: " << e.what() << std::endl;
        return false;
    }
}

bool ConfigParser::loadFromString(const std::string& jsonStr) {
    try {
        Json::CharReaderBuilder builder;
        Json::Value root;
        std::string errs;
        std::istringstream stream(jsonStr);
        
        if (!Json::parseFromStream(builder, stream, &root, &errs)) {
            std::cerr << "Failed to parse JSON string: " << errs << std::endl;
            return false;
        }
        
        auto& configHelper = ConfigHelper::getInstance();
        
        // 解析各个配置段（与 loadFromFile 相同逻辑）
        if (root.isMember("stream")) {
            parseStreamConfig(root["stream"], configHelper.streamConfig);
        }
        if (root.isMember("render")) {
            parseRenderConfig(root["render"], configHelper.renderConfig);
        }
        if (root.isMember("save")) {
            parseSaveConfig(root["save"], configHelper.saveConfig);
        }
        if (root.isMember("metadata")) {
            parseMetadataConfig(root["metadata"], configHelper.metadataConfig);
        }
        if (root.isMember("hotplug")) {
            parseHotPlugConfig(root["hotplug"], configHelper.hotPlugConfig);
        }
        if (root.isMember("parallel")) {
            parseParallelConfig(root["parallel"], configHelper.parallelConfig);
        }
        if (root.isMember("inference")) {
            parseInferenceConfig(root["inference"], configHelper.inferenceConfig);
        }
        if (root.isMember("calibration")) {
            parseCalibrationConfig(root["calibration"], configHelper.calibrationConfig);
        }
        if (root.isMember("logger")) {
            parseLoggerConfig(root["logger"], configHelper.loggerConfig);
        }
        
        std::cout << "Configuration loaded successfully from JSON string" << std::endl;
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Exception while loading config from string: " << e.what() << std::endl;
        return false;
    }
}

std::string ConfigParser::saveToString() {
    try {
        auto& configHelper = ConfigHelper::getInstance();
        
        Json::Value root;
        root["stream"] = streamConfigToJson(configHelper.streamConfig);
        root["render"] = renderConfigToJson(configHelper.renderConfig);
        root["save"] = saveConfigToJson(configHelper.saveConfig);
        root["metadata"] = metadataConfigToJson(configHelper.metadataConfig);
        root["hotplug"] = hotPlugConfigToJson(configHelper.hotPlugConfig);
        root["parallel"] = parallelConfigToJson(configHelper.parallelConfig);
        root["inference"] = inferenceConfigToJson(configHelper.inferenceConfig);
        root["calibration"] = calibrationConfigToJson(configHelper.calibrationConfig);
        root["logger"] = loggerConfigToJson(configHelper.loggerConfig);
        
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "  ";
        return Json::writeString(builder, root);
    }
    catch (const std::exception& e) {
        std::cerr << "Exception while converting config to string: " << e.what() << std::endl;
        return "";
    }
}

// =================== 模板特化实现 ===================

template<>
bool ConfigParser::safeGetValue<bool>(const Json::Value& json, const std::string& key, const bool& defaultValue) {
    return json.isMember(key) && json[key].isBool() ? json[key].asBool() : defaultValue;
}

template<>
int ConfigParser::safeGetValue<int>(const Json::Value& json, const std::string& key, const int& defaultValue) {
    return json.isMember(key) && json[key].isInt() ? json[key].asInt() : defaultValue;
}

template<>
float ConfigParser::safeGetValue<float>(const Json::Value& json, const std::string& key, const float& defaultValue) {
    return json.isMember(key) && json[key].isNumeric() ? json[key].asFloat() : defaultValue;
}

template<>
double ConfigParser::safeGetValue<double>(const Json::Value& json, const std::string& key, const double& defaultValue) {
    return json.isMember(key) && json[key].isNumeric() ? json[key].asDouble() : defaultValue;
}

template<>
std::string ConfigParser::safeGetValue<std::string>(const Json::Value& json, const std::string& key, const std::string& defaultValue) {
    return json.isMember(key) && json[key].isString() ? json[key].asString() : defaultValue;
}

// =================== 解析方法实现 ===================

void ConfigParser::parseStreamConfig(const Json::Value& json, ConfigHelper::StreamConfig& config) {
    config.enableColor = safeGetValue(json, "enableColor", config.enableColor);
    config.enableDepth = safeGetValue(json, "enableDepth", config.enableDepth);
    config.enableIR = safeGetValue(json, "enableIR", config.enableIR);
    config.enableIRLeft = safeGetValue(json, "enableIRLeft", config.enableIRLeft);
    config.enableIRRight = safeGetValue(json, "enableIRRight", config.enableIRRight);
    config.enableIMU = safeGetValue(json, "enableIMU", config.enableIMU);
    config.colorWidth = safeGetValue(json, "colorWidth", config.colorWidth);
    config.colorHeight = safeGetValue(json, "colorHeight", config.colorHeight);
    config.colorFPS = safeGetValue(json, "colorFPS", config.colorFPS);
    config.depthWidth = safeGetValue(json, "depthWidth", config.depthWidth);
    config.depthHeight = safeGetValue(json, "depthHeight", config.depthHeight);
    config.depthFPS = safeGetValue(json, "depthFPS", config.depthFPS);
}

void ConfigParser::parseRenderConfig(const Json::Value& json, ConfigHelper::RenderConfig& config) {
    config.enableRendering = safeGetValue(json, "enableRendering", config.enableRendering);
    config.windowWidth = safeGetValue(json, "windowWidth", config.windowWidth);
    config.windowHeight = safeGetValue(json, "windowHeight", config.windowHeight);
    config.showFPS = safeGetValue(json, "showFPS", config.showFPS);
    config.autoResize = safeGetValue(json, "autoResize", config.autoResize);
    config.windowTitle = safeGetValue(json, "windowTitle", config.windowTitle);
}

void ConfigParser::parseSaveConfig(const Json::Value& json, ConfigHelper::SaveConfig& config) {
    config.enableDump = safeGetValue(json, "enableDump", config.enableDump);
    config.dumpPath = safeGetValue(json, "dumpPath", config.dumpPath);
    config.saveColor = safeGetValue(json, "saveColor", config.saveColor);
    config.saveDepth = safeGetValue(json, "saveDepth", config.saveDepth);
    config.saveDepthColormap = safeGetValue(json, "saveDepthColormap", config.saveDepthColormap);
    config.saveDepthData = safeGetValue(json, "saveDepthData", config.saveDepthData);
    config.saveIR = safeGetValue(json, "saveIR", config.saveIR);
    config.savePointCloud = safeGetValue(json, "savePointCloud", config.savePointCloud);
    config.saveMetadata = safeGetValue(json, "saveMetadata", config.saveMetadata);
    config.enableMetadataConsole = safeGetValue(json, "enableMetadataConsole", config.enableMetadataConsole);
    config.imageFormat = safeGetValue(json, "imageFormat", config.imageFormat);
    config.maxFramesToSave = safeGetValue(json, "maxFramesToSave", config.maxFramesToSave);
    config.frameInterval = safeGetValue(json, "frameInterval", config.frameInterval);
    config.enableFrameStats = safeGetValue(json, "enableFrameStats", config.enableFrameStats);
}

void ConfigParser::parseMetadataConfig(const Json::Value& json, ConfigHelper::MetadataConfig& config) {
    config.showTimestamp = safeGetValue(json, "showTimestamp", config.showTimestamp);
    config.showFrameNumber = safeGetValue(json, "showFrameNumber", config.showFrameNumber);
    config.showDeviceInfo = safeGetValue(json, "showDeviceInfo", config.showDeviceInfo);
}

void ConfigParser::parseHotPlugConfig(const Json::Value& json, ConfigHelper::HotPlugConfig& config) {
    config.enableHotPlug = safeGetValue(json, "enableHotPlug", config.enableHotPlug);
    config.autoReconnect = safeGetValue(json, "autoReconnect", config.autoReconnect);
    config.printDeviceEvents = safeGetValue(json, "printDeviceEvents", config.printDeviceEvents);
    config.reconnectDelayMs = safeGetValue(json, "reconnectDelayMs", config.reconnectDelayMs);
    config.maxReconnectAttempts = safeGetValue(json, "maxReconnectAttempts", config.maxReconnectAttempts);
    config.deviceStabilizeDelayMs = safeGetValue(json, "deviceStabilizeDelayMs", config.deviceStabilizeDelayMs);
    config.waitForDeviceOnStartup = safeGetValue(json, "waitForDeviceOnStartup", config.waitForDeviceOnStartup);
}

void ConfigParser::parseParallelConfig(const Json::Value& json, ConfigHelper::ParallelConfig& config) {
    config.enableParallelProcessing = safeGetValue(json, "enableParallelProcessing", config.enableParallelProcessing);
    config.threadPoolSize = safeGetValue(json, "threadPoolSize", config.threadPoolSize);
    config.maxQueuedTasks = safeGetValue(json, "maxQueuedTasks", config.maxQueuedTasks);
}

void ConfigParser::parseInferenceConfig(const Json::Value& json, ConfigHelper::InferenceConfig& config) {
    config.enableInference = safeGetValue(json, "enableInference", config.enableInference);
    config.defaultModel = safeGetValue(json, "defaultModel", config.defaultModel);
    config.defaultModelType = safeGetValue(json, "defaultModelType", config.defaultModelType);
    config.defaultThreshold = safeGetValue(json, "defaultThreshold", config.defaultThreshold);
    config.enableVisualization = safeGetValue(json, "enableVisualization", config.enableVisualization);
    config.enablePerformanceStats = safeGetValue(json, "enablePerformanceStats", config.enablePerformanceStats);
    config.inferenceInterval = safeGetValue(json, "inferenceInterval", config.inferenceInterval);
    config.classNamesFile = safeGetValue(json, "classNamesFile", config.classNamesFile);
    config.asyncInference = safeGetValue(json, "asyncInference", config.asyncInference);
    config.maxQueueSize = safeGetValue(json, "maxQueueSize", config.maxQueueSize);
    config.modelsDirectory = safeGetValue(json, "modelsDirectory", config.modelsDirectory);
    config.enableFramePreprocessing = safeGetValue(json, "enableFramePreprocessing", config.enableFramePreprocessing);
    config.onlyProcessColorFrames = safeGetValue(json, "onlyProcessColorFrames", config.onlyProcessColorFrames);
}

void ConfigParser::parseCalibrationConfig(const Json::Value& json, ConfigHelper::CalibrationConfig& config) {
    config.enableCalibration = safeGetValue(json, "enableCalibration", config.enableCalibration);
    config.boardWidth = safeGetValue(json, "boardWidth", config.boardWidth);
    config.boardHeight = safeGetValue(json, "boardHeight", config.boardHeight);
    config.squareSize = safeGetValue(json, "squareSize", config.squareSize);
    config.minValidFrames = safeGetValue(json, "minValidFrames", config.minValidFrames);
    config.maxFrames = safeGetValue(json, "maxFrames", config.maxFrames);
    config.minInterval = safeGetValue(json, "minInterval", config.minInterval);
    config.useSubPixel = safeGetValue(json, "useSubPixel", config.useSubPixel);
    config.enableUndistortion = safeGetValue(json, "enableUndistortion", config.enableUndistortion);
    config.saveDirectory = safeGetValue(json, "saveDirectory", config.saveDirectory);
    config.autoStartCalibrationOnStartup = safeGetValue(json, "autoStartCalibrationOnStartup", config.autoStartCalibrationOnStartup);
    config.showCalibrationProgress = safeGetValue(json, "showCalibrationProgress", config.showCalibrationProgress);
}

void ConfigParser::parseLoggerConfig(const Json::Value& json, ConfigHelper::LoggerConfig& config) {
    // Logger::Level 需要特殊处理
    if (json.isMember("logLevel") && json["logLevel"].isInt()) {
        int level = json["logLevel"].asInt();
        if (level >= 0 && level <= 4) { // 假设有5个日志级别
            config.logLevel = static_cast<Logger::Level>(level);
        }
    }
    config.enableConsole = safeGetValue(json, "enableConsole", config.enableConsole);
    config.enableFileLogging = safeGetValue(json, "enableFileLogging", config.enableFileLogging);
    config.logDirectory = safeGetValue(json, "logDirectory", config.logDirectory);
}

// =================== 序列化方法实现 ===================

Json::Value ConfigParser::streamConfigToJson(const ConfigHelper::StreamConfig& config) {
    Json::Value json;
    json["enableColor"] = config.enableColor;
    json["enableDepth"] = config.enableDepth;
    json["enableIR"] = config.enableIR;
    json["enableIRLeft"] = config.enableIRLeft;
    json["enableIRRight"] = config.enableIRRight;
    json["enableIMU"] = config.enableIMU;
    json["colorWidth"] = config.colorWidth;
    json["colorHeight"] = config.colorHeight;
    json["colorFPS"] = config.colorFPS;
    json["depthWidth"] = config.depthWidth;
    json["depthHeight"] = config.depthHeight;
    json["depthFPS"] = config.depthFPS;
    return json;
}

Json::Value ConfigParser::renderConfigToJson(const ConfigHelper::RenderConfig& config) {
    Json::Value json;
    json["enableRendering"] = config.enableRendering;
    json["windowWidth"] = config.windowWidth;
    json["windowHeight"] = config.windowHeight;
    json["showFPS"] = config.showFPS;
    json["autoResize"] = config.autoResize;
    json["windowTitle"] = config.windowTitle;
    return json;
}

Json::Value ConfigParser::saveConfigToJson(const ConfigHelper::SaveConfig& config) {
    Json::Value json;
    json["enableDump"] = config.enableDump;
    json["dumpPath"] = config.dumpPath;
    json["saveColor"] = config.saveColor;
    json["saveDepth"] = config.saveDepth;
    json["saveDepthColormap"] = config.saveDepthColormap;
    json["saveDepthData"] = config.saveDepthData;
    json["saveIR"] = config.saveIR;
    json["savePointCloud"] = config.savePointCloud;
    json["saveMetadata"] = config.saveMetadata;
    json["enableMetadataConsole"] = config.enableMetadataConsole;
    json["imageFormat"] = config.imageFormat;
    json["maxFramesToSave"] = config.maxFramesToSave;
    json["frameInterval"] = config.frameInterval;
    json["enableFrameStats"] = config.enableFrameStats;
    return json;
}

Json::Value ConfigParser::metadataConfigToJson(const ConfigHelper::MetadataConfig& config) {
    Json::Value json;
    json["showTimestamp"] = config.showTimestamp;
    json["showFrameNumber"] = config.showFrameNumber;
    json["showDeviceInfo"] = config.showDeviceInfo;
    return json;
}

Json::Value ConfigParser::hotPlugConfigToJson(const ConfigHelper::HotPlugConfig& config) {
    Json::Value json;
    json["enableHotPlug"] = config.enableHotPlug;
    json["autoReconnect"] = config.autoReconnect;
    json["printDeviceEvents"] = config.printDeviceEvents;
    json["reconnectDelayMs"] = config.reconnectDelayMs;
    json["maxReconnectAttempts"] = config.maxReconnectAttempts;
    json["deviceStabilizeDelayMs"] = config.deviceStabilizeDelayMs;
    json["waitForDeviceOnStartup"] = config.waitForDeviceOnStartup;
    return json;
}

Json::Value ConfigParser::parallelConfigToJson(const ConfigHelper::ParallelConfig& config) {
    Json::Value json;
    json["enableParallelProcessing"] = config.enableParallelProcessing;
    json["threadPoolSize"] = config.threadPoolSize;
    json["maxQueuedTasks"] = config.maxQueuedTasks;
    return json;
}

Json::Value ConfigParser::inferenceConfigToJson(const ConfigHelper::InferenceConfig& config) {
    Json::Value json;
    json["enableInference"] = config.enableInference;
    json["defaultModel"] = config.defaultModel;
    json["defaultModelType"] = config.defaultModelType;
    json["defaultThreshold"] = config.defaultThreshold;
    json["enableVisualization"] = config.enableVisualization;
    json["enablePerformanceStats"] = config.enablePerformanceStats;
    json["inferenceInterval"] = config.inferenceInterval;
    json["classNamesFile"] = config.classNamesFile;
    json["asyncInference"] = config.asyncInference;
    json["maxQueueSize"] = config.maxQueueSize;
    json["modelsDirectory"] = config.modelsDirectory;
    json["enableFramePreprocessing"] = config.enableFramePreprocessing;
    json["onlyProcessColorFrames"] = config.onlyProcessColorFrames;
    return json;
}

Json::Value ConfigParser::calibrationConfigToJson(const ConfigHelper::CalibrationConfig& config) {
    Json::Value json;
    json["enableCalibration"] = config.enableCalibration;
    json["boardWidth"] = config.boardWidth;
    json["boardHeight"] = config.boardHeight;
    json["squareSize"] = config.squareSize;
    json["minValidFrames"] = config.minValidFrames;
    json["maxFrames"] = config.maxFrames;
    json["minInterval"] = config.minInterval;
    json["useSubPixel"] = config.useSubPixel;
    json["enableUndistortion"] = config.enableUndistortion;
    json["saveDirectory"] = config.saveDirectory;
    json["autoStartCalibrationOnStartup"] = config.autoStartCalibrationOnStartup;
    json["showCalibrationProgress"] = config.showCalibrationProgress;
    return json;
}

Json::Value ConfigParser::loggerConfigToJson(const ConfigHelper::LoggerConfig& config) {
    Json::Value json;
    json["logLevel"] = static_cast<int>(config.logLevel);
    json["enableConsole"] = config.enableConsole;
    json["enableFileLogging"] = config.enableFileLogging;
    json["logDirectory"] = config.logDirectory;
    return json;
} 