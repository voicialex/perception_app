#pragma once

#include <string>

class ConfigHelper {
public:
    static ConfigHelper& getInstance() {
        static ConfigHelper instance;
        return instance;
    }

    // 数据流配置
    struct StreamConfig {
        bool enableColor = true;
        bool enableDepth = true;
        bool enableIR = false;
        bool enableIRLeft = false;
        bool enableIRRight = false;
        bool enableIMU = false;
    } streamConfig;

    // 渲染配置
    struct RenderConfig {
        bool enableRendering = true;
        int windowWidth = 1280;
        int windowHeight = 720;
    } renderConfig;

    // 数据保存配置
    struct SaveConfig {
        bool enableDump = false;
        std::string dumpPath = "./";
    } saveConfig;

    // 元数据显示配置
    struct MetadataConfig {
        bool enableMetadata = true;
        int printInterval = 300;  // 每30帧打印一次元数据 30fps
    } metadataConfig;

private:
    ConfigHelper() = default;
    ~ConfigHelper() = default;
    ConfigHelper(const ConfigHelper&) = delete;
    ConfigHelper& operator=(const ConfigHelper&) = delete;
}; 