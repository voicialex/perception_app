#pragma once

#include <memory>
#include <string>
#include <vector>
#include <opencv2/opencv.hpp>
#include "libobsensor/ObSensor.hpp"

namespace inference {

/**
 * @brief 推理结果的基类
 */
class InferenceResult {
public:
    virtual ~InferenceResult() = default;
    
    /**
     * @brief 获取推理执行时间
     * @return 推理时间(毫秒)
     */
    virtual double getInferenceTime() const = 0;
    
    /**
     * @brief 检查结果是否有效
     * @return 是否有效
     */
    virtual bool isValid() const = 0;
    
    /**
     * @brief 获取结果摘要
     * @return 结果摘要字符串
     */
    virtual std::string getSummary() const = 0;
};

/**
 * @brief 推理模型的配置信息
 */
struct ModelConfig {
    std::string modelPath;              // 模型文件路径
    std::string modelType;              // 模型类型: "classification", "detection", "segmentation"
    std::string engineType = "onnx";    // 推理引擎类型
    std::vector<std::string> classNames; // 类别名称
    float confidenceThreshold = 0.5f;   // 置信度阈值
    std::vector<int64_t> inputShape;    // 输入形状 (可选)
    std::vector<std::string> inputNames; // 输入名称 (可选)
    std::vector<std::string> outputNames; // 输出名称 (可选)
    
    bool isValid() const {
        return !modelPath.empty() && !modelType.empty() && !engineType.empty();
    }
};

/**
 * @brief 推理引擎的基类
 */
class InferenceEngine {
public:
    virtual ~InferenceEngine() = default;

    /**
     * @brief 初始化推理引擎
     * @param config 模型配置
     * @return 是否初始化成功
     */
    virtual bool initialize(const ModelConfig& config) = 0;

    /**
     * @brief 执行推理
     * @param inputImage 输入图像
     * @return 推理结果
     */
    virtual std::shared_ptr<InferenceResult> infer(const cv::Mat& inputImage) = 0;

    /**
     * @brief 获取模型信息
     * @return 模型信息字符串
     */
    virtual std::string getModelInfo() const = 0;

    /**
     * @brief 设置阈值参数
     * @param threshold 阈值值
     */
    virtual void setThreshold(float threshold) = 0;

    /**
     * @brief 检查引擎是否已初始化
     * @return 是否已初始化
     */
    virtual bool isInitialized() const = 0;
    
    /**
     * @brief 获取模型类型
     * @return 模型类型
     */
    virtual std::string getModelType() const = 0;
};

/**
 * @brief 推理引擎工厂类
 */
class InferenceEngineFactory {
public:
    /**
     * @brief 创建推理引擎
     * @param engineType 引擎类型 ("onnx", "tensorrt", 等)
     * @return 推理引擎指针
     */
    static std::shared_ptr<InferenceEngine> createEngine(const std::string& engineType);
};

} // namespace inference 