#include "config/ConfigParser.hpp"
#include "config/ConfigHelper.hpp"
#include <iostream>

int main() {
    std::cout << "=== ConfigParser 使用示例 ===" << std::endl;
    
    // 获取 ConfigHelper 实例
    auto& config = ConfigHelper::getInstance();
    
    // 1. 加载配置文件
    std::cout << "\n1. 从文件加载配置..." << std::endl;
    if (ConfigParser::loadFromFile("config.json")) {
        std::cout << "配置加载成功！" << std::endl;
        
        // 显示加载后的配置
        std::cout << "彩色流启用: " << (config.streamConfig.enableColor ? "是" : "否") << std::endl;
        std::cout << "深度流启用: " << (config.streamConfig.enableDepth ? "是" : "否") << std::endl;
        std::cout << "窗口尺寸: " << config.renderConfig.windowWidth 
                  << "x" << config.renderConfig.windowHeight << std::endl;
        std::cout << "推理启用: " << (config.inferenceConfig.enableInference ? "是" : "否") << std::endl;
        std::cout << "标定启用: " << (config.calibrationConfig.enableCalibration ? "是" : "否") << std::endl;
    } else {
        std::cout << "配置加载失败！" << std::endl;
    }
    
    // 2. 修改配置
    std::cout << "\n2. 修改配置..." << std::endl;
    config.streamConfig.colorWidth = 1920;
    config.streamConfig.colorHeight = 1080;
    config.inferenceConfig.enableInference = true;
    config.inferenceConfig.defaultThreshold = 0.8f;
    config.calibrationConfig.enableCalibration = true;
    config.calibrationConfig.boardWidth = 11;
    config.calibrationConfig.boardHeight = 8;
    
    // 3. 保存配置到新文件
    std::cout << "\n3. 保存修改后的配置..." << std::endl;
    if (ConfigParser::saveToFile("config_modified.json")) {
        std::cout << "配置保存成功！" << std::endl;
    } else {
        std::cout << "配置保存失败！" << std::endl;
    }
    
    // 4. 导出为 JSON 字符串
    std::cout << "\n4. 导出为 JSON 字符串..." << std::endl;
    std::string jsonStr = ConfigParser::saveToString();
    if (!jsonStr.empty()) {
        std::cout << "JSON 导出成功，长度: " << jsonStr.length() << " 字符" << std::endl;
        // 只显示前200个字符以避免输出过长
        std::cout << "前200字符: " << jsonStr.substr(0, 200) << "..." << std::endl;
    } else {
        std::cout << "JSON 导出失败！" << std::endl;
    }
    
    // 5. 从 JSON 字符串加载（演示部分配置）
    std::cout << "\n5. 从 JSON 字符串加载部分配置..." << std::endl;
    std::string partialConfig = R"({
        "stream": {
            "colorWidth": 640,
            "colorHeight": 480,
            "enableIR": true
        },
        "calibration": {
            "enableCalibration": true,
            "boardWidth": 7,
            "boardHeight": 5,
            "squareSize": 30.0
        }
    })";
    
    if (ConfigParser::loadFromString(partialConfig)) {
        std::cout << "部分配置加载成功！" << std::endl;
        std::cout << "新的彩色流尺寸: " << config.streamConfig.colorWidth 
                  << "x" << config.streamConfig.colorHeight << std::endl;
        std::cout << "红外流启用: " << (config.streamConfig.enableIR ? "是" : "否") << std::endl;
        std::cout << "标定板尺寸: " << config.calibrationConfig.boardWidth 
                  << "x" << config.calibrationConfig.boardHeight << std::endl;
        std::cout << "方格大小: " << config.calibrationConfig.squareSize << "mm" << std::endl;
    } else {
        std::cout << "部分配置加载失败！" << std::endl;
    }
    
    std::cout << "\n=== 示例完成 ===" << std::endl;
    return 0;
} 