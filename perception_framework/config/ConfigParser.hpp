#pragma once

#include <string>
#include <json/json.h>
#include "ConfigHelper.hpp"

/**
 * @brief 动态配置解析器
 * 专门负责从 JSON 文件读取配置并应用到 ConfigHelper
 */
class ConfigParser {
public:
    /**
     * @brief 从JSON文件加载所有配置
     * @param filepath JSON配置文件路径
     * @return 是否成功加载
     */
    static bool loadFromFile(const std::string& filepath);
    
    /**
     * @brief 保存所有配置到JSON文件
     * @param filepath JSON配置文件路径
     * @return 是否成功保存
     */
    static bool saveToFile(const std::string& filepath);
    
    /**
     * @brief 从JSON字符串加载配置
     * @param jsonStr JSON字符串
     * @return 是否成功加载
     */
    static bool loadFromString(const std::string& jsonStr);
    
    /**
     * @brief 将当前配置导出为JSON字符串
     * @return JSON字符串
     */
    static std::string saveToString();

private:
    /**
     * @brief 解析流配置
     */
    static void parseStreamConfig(const Json::Value& json, ConfigHelper::StreamConfig& config);
    
    /**
     * @brief 解析渲染配置
     */
    static void parseRenderConfig(const Json::Value& json, ConfigHelper::RenderConfig& config);
    
    /**
     * @brief 解析保存配置
     */
    static void parseSaveConfig(const Json::Value& json, ConfigHelper::SaveConfig& config);
    
    /**
     * @brief 解析元数据配置
     */
    static void parseMetadataConfig(const Json::Value& json, ConfigHelper::MetadataConfig& config);
    
    /**
     * @brief 解析热插拔配置
     */
    static void parseHotPlugConfig(const Json::Value& json, ConfigHelper::HotPlugConfig& config);
    
    /**
     * @brief 解析并行处理配置
     */
    static void parseParallelConfig(const Json::Value& json, ConfigHelper::ParallelConfig& config);
    
    /**
     * @brief 解析推理配置
     */
    static void parseInferenceConfig(const Json::Value& json, ConfigHelper::InferenceConfig& config);
    
    /**
     * @brief 解析标定配置
     */
    static void parseCalibrationConfig(const Json::Value& json, ConfigHelper::CalibrationConfig& config);
    
    /**
     * @brief 解析日志配置
     */
    static void parseLoggerConfig(const Json::Value& json, ConfigHelper::LoggerConfig& config);
    
    // 序列化方法
    static Json::Value streamConfigToJson(const ConfigHelper::StreamConfig& config);
    static Json::Value renderConfigToJson(const ConfigHelper::RenderConfig& config);
    static Json::Value saveConfigToJson(const ConfigHelper::SaveConfig& config);
    static Json::Value metadataConfigToJson(const ConfigHelper::MetadataConfig& config);
    static Json::Value hotPlugConfigToJson(const ConfigHelper::HotPlugConfig& config);
    static Json::Value parallelConfigToJson(const ConfigHelper::ParallelConfig& config);
    static Json::Value inferenceConfigToJson(const ConfigHelper::InferenceConfig& config);
    static Json::Value calibrationConfigToJson(const ConfigHelper::CalibrationConfig& config);
    static Json::Value loggerConfigToJson(const ConfigHelper::LoggerConfig& config);
    
    /**
     * @brief 安全获取JSON值的辅助方法
     */
    template<typename T>
    static T safeGetValue(const Json::Value& json, const std::string& key, const T& defaultValue);
}; 