#include <iostream>
#include <opencv2/opencv.hpp>
#include <thread>
#include <chrono>
#include "inference/InferenceManager.hpp"
#include "calibration/CalibrationManager.hpp"
#include "utils/Logger.hpp"
#include "core/ConfigHelper.hpp"

int main(int argc, char* argv[]) {
    (void)argc; // 避免未使用参数警告
    (void)argv; // 避免未使用参数警告
    
    // 初始化日志系统
    Logger::getInstance().setLevel("INFO");
    
    LOG_INFO("推理演示程序启动");
    
    try {
        // 初始化配置
        auto& config = ConfigHelper::getInstance();
        
        // 配置推理系统
        config.inferenceConfig.enableInference = true;
        config.inferenceConfig.defaultModel = "demo_model.onnx";
        config.inferenceConfig.defaultModelType = "classification";
        config.inferenceConfig.defaultThreshold = 0.5f;
        config.inferenceConfig.enableVisualization = true;
        config.inferenceConfig.enablePerformanceStats = true;
        config.inferenceConfig.asyncInference = false;
        
        // 获取推理管理器
        auto& inferenceManager = inference::InferenceManager::getInstance();
        
        // 初始化推理系统
        inference::InferenceConfig inferenceConfig;
        inferenceConfig.enableInference = true;
        inferenceConfig.defaultModel = "demo_model.onnx";
        inferenceConfig.defaultModelType = "classification";
        inferenceConfig.defaultThreshold = 0.5f;
        inferenceConfig.enableVisualization = true;
        inferenceConfig.enablePerformanceStats = true;
        inferenceConfig.asyncInference = false;
        
        if (!inferenceManager.initialize(inferenceConfig)) {
            LOG_ERROR("推理管理器初始化失败");
            return -1;
        }
        
        LOG_INFO("推理管理器初始化成功");
        
        // 创建测试图像
        cv::Mat testImage = cv::Mat::zeros(640, 480, CV_8UC3);
        cv::putText(testImage, "Test Image", cv::Point(100, 300), 
                   cv::FONT_HERSHEY_SIMPLEX, 2, cv::Scalar(255, 255, 255), 2);
        
        // 执行推理测试 - 注意：processImageSync不存在，使用runInference
        try {
            // 这里需要提供一个有效的模型名称，但由于没有实际加载模型，会失败
            LOG_INFO("跳过推理测试 - 需要实际的模型文件");
        } catch (const std::exception& e) {
            LOG_ERROR("推理测试失败: ", e.what());
        }
        
        // 标定系统测试
        LOG_INFO("开始相机标定系统测试");
        
        auto& calibrationManager = calibration::CalibrationManager::getInstance();
        
        // 初始化标定管理器
        if (!calibrationManager.initialize()) {
            LOG_ERROR("标定管理器初始化失败");
            return -1;
        }
        
        // 设置标定配置
        calibration::CalibrationConfig calibConfig;
        calibConfig.boardWidth = 9;
        calibConfig.boardHeight = 6;
        calibConfig.squareSize = 25.0f;
        calibConfig.minValidFrames = 10;
        calibConfig.maxFrames = 20;
        calibConfig.minInterval = 1;
        calibConfig.useSubPixel = true;
        calibConfig.enableUndistortion = true;
        calibConfig.saveDirectory = "./calibration/";
        
        // 设置进度回调
        auto progressCallback = [](calibration::CalibrationState state, int current, 
                                 int total, const std::string& message) {
            LOG_INFO("标定进度: ", static_cast<int>(state), " (", current, "/", total, ") - ", message);
        };
        
        // 启动标定
        if (!calibrationManager.startCalibration(calibConfig, progressCallback)) {
            LOG_ERROR("启动标定失败");
            return -1;
        }
        
        // 创建模拟棋盘图像
        cv::Mat chessboardImage = cv::Mat::zeros(480, 640, CV_8UC3);
        // 简单的棋盘模式
        for (int i = 0; i < 6; ++i) {
            for (int j = 0; j < 9; ++j) {
                if ((i + j) % 2 == 0) {
                    cv::Rect rect(j * 60 + 50, i * 60 + 50, 60, 60);
                    cv::rectangle(chessboardImage, rect, cv::Scalar(255, 255, 255), -1);
                }
            }
        }
        
        // 处理几帧标定图像
        LOG_INFO("处理标定图像...");
        for (int i = 0; i < 3; ++i) {
            calibrationManager.processFrame(chessboardImage);
            std::this_thread::sleep_for(std::chrono::milliseconds(1100)); // 等待超过最小间隔
        }
        
        // 显示测试图像
        cv::imshow("推理演示 - 测试图像", testImage);
        cv::imshow("推理演示 - 标定棋盘", chessboardImage);
        
        LOG_INFO("演示完成，按任意键退出");
        cv::waitKey(0);
        cv::destroyAllWindows();
        
        // 清理
        inferenceManager.stop();
        calibrationManager.stop();
        
        LOG_INFO("推理演示程序正常退出");
        return 0;
        
    } catch (const std::exception& e) {
        LOG_ERROR("程序异常: ", e.what());
        return -1;
    }
} 