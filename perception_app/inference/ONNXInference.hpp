#pragma once

#include "InferenceBase.hpp"
#include <opencv2/opencv.hpp>
#include <vector>
#include <memory>

namespace inference {

/**
 * @brief 分类结果
 */
struct ClassificationResult {
    int classId = -1;
    float confidence = 0.0f;
    std::string className;
};

/**
 * @brief 检测框结果
 */
struct DetectionBox {
    cv::Rect2f bbox;
    int classId = -1;
    float confidence = 0.0f;
    std::string className;
};

/**
 * @brief ONNX推理结果
 */
class ONNXInferenceResult : public InferenceResult {
public:
    ONNXInferenceResult();
    virtual ~ONNXInferenceResult() = default;
    
    double getInferenceTime() const override { return inferenceTime_; }
    bool isValid() const override { return valid_; }
    std::string getSummary() const override;
    
    void setInferenceTime(double time) { inferenceTime_ = time; }
    void setValid(bool valid) { valid_ = valid; }
    
    // 分类结果
    void setClassificationResult(const ClassificationResult& result) {
        classificationResult_ = result;
        resultType_ = "classification";
    }
    
    // 检测结果
    void setDetectionResults(const std::vector<DetectionBox>& results) {
        detectionResults_ = results;
        resultType_ = "detection";
    }
    
    // 分割结果
    void setSegmentationMask(const cv::Mat& mask) {
        segmentationMask_ = mask;
        resultType_ = "segmentation";
    }
    
    // 获取结果
    const ClassificationResult& getClassificationResult() const { return classificationResult_; }
    const std::vector<DetectionBox>& getDetectionResults() const { return detectionResults_; }
    const cv::Mat& getSegmentationMask() const { return segmentationMask_; }
    const std::string& getResultType() const { return resultType_; }

private:
    double inferenceTime_ = 0.0;
    bool valid_ = false;
    std::string resultType_;
    
    ClassificationResult classificationResult_;
    std::vector<DetectionBox> detectionResults_;
    cv::Mat segmentationMask_;
};

/**
 * @brief ONNX推理引擎
 */
class ONNXInferenceEngine : public InferenceEngine {
public:
    ONNXInferenceEngine();
    virtual ~ONNXInferenceEngine();
    
    bool initialize(const ModelConfig& config) override;
    std::shared_ptr<InferenceResult> infer(const cv::Mat& inputImage) override;
    std::string getModelInfo() const override;
    void setThreshold(float threshold) override { threshold_ = threshold; }
    bool isInitialized() const override { return initialized_; }
    std::string getModelType() const override { return modelType_; }

private:
    /**
     * @brief 预处理输入图像
     * @param image 输入图像
     * @return 预处理后的数据
     */
    std::vector<float> preprocessImage(const cv::Mat& image);
    
    /**
     * @brief 后处理分类结果
     * @param output 模型输出
     * @return 分类结果
     */
    ClassificationResult postprocessClassification(const std::vector<float>& output);
    
    /**
     * @brief 后处理检测结果
     * @param output 模型输出
     * @param imageSize 原始图像大小
     * @return 检测结果
     */
    std::vector<DetectionBox> postprocessDetection(const std::vector<float>& output, 
                                                   const cv::Size& imageSize);
    
    /**
     * @brief 后处理分割结果
     * @param output 模型输出
     * @param imageSize 原始图像大小
     * @return 分割掩码
     */
    cv::Mat postprocessSegmentation(const std::vector<float>& output, 
                                   const cv::Size& imageSize);
    
    /**
     * @brief 非极大值抑制
     * @param boxes 检测框
     * @return 过滤后的检测框
     */
    std::vector<DetectionBox> applyNMS(const std::vector<DetectionBox>& boxes);

private:
    bool initialized_ = false;
    std::string modelPath_;
    std::string modelType_;
    std::vector<std::string> classNames_;
    float threshold_ = 0.5f;
    
    // 模型输入输出信息
    cv::Size inputSize_ = cv::Size(640, 640);
    std::vector<int64_t> inputShape_;
    std::vector<std::string> inputNames_;
    std::vector<std::string> outputNames_;
    
    // 模拟的模型会话（实际应该是ONNX Runtime会话）
    void* session_ = nullptr;
};

} // namespace inference 