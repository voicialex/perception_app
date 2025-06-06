#include "ONNXInference.hpp"
#include "Logger.hpp"
#include <random>
#include <chrono>

namespace inference {

// ONNXInferenceResult 实现
ONNXInferenceResult::ONNXInferenceResult() {
    // 构造函数
}

std::string ONNXInferenceResult::getSummary() const {
    if (!valid_) {
        return "Invalid result";
    }
    
    std::ostringstream oss;
    oss << "Type: " << resultType_ << ", Time: " << inferenceTime_ << "ms";
    
    if (resultType_ == "classification") {
        oss << ", Class: " << classificationResult_.className 
            << " (" << classificationResult_.confidence << ")";
    } else if (resultType_ == "detection") {
        oss << ", Detections: " << detectionResults_.size();
    } else if (resultType_ == "segmentation") {
        oss << ", Mask: " << segmentationMask_.size();
    }
    
    return oss.str();
}

// ONNXInferenceEngine 实现
ONNXInferenceEngine::ONNXInferenceEngine() {
    LOG_DEBUG("ONNXInferenceEngine created");
}

ONNXInferenceEngine::~ONNXInferenceEngine() {
    if (session_) {
        // 清理ONNX Runtime会话（这里是模拟）
        session_ = nullptr;
    }
    LOG_DEBUG("ONNXInferenceEngine destroyed");
}

bool ONNXInferenceEngine::initialize(const ModelConfig& config) {
    try {
        if (!config.isValid()) {
            LOG_ERROR("Invalid model configuration");
            return false;
        }
        
        modelPath_ = config.modelPath;
        modelType_ = config.modelType;
        classNames_ = config.classNames;
        threshold_ = config.confidenceThreshold;
        
        // 这里应该是真正的ONNX Runtime初始化
        // 为了演示，我们创建一个模拟的会话
        session_ = reinterpret_cast<void*>(0x12345678); // 模拟指针
        
        // 设置输入形状（默认值）
        if (!config.inputShape.empty()) {
            inputShape_ = config.inputShape;
        } else {
            inputShape_ = {1, 3, 640, 640}; // NCHW格式
        }
        
        // 设置输入输出名称
        if (!config.inputNames.empty()) {
            inputNames_ = config.inputNames;
        } else {
            inputNames_ = {"input"};
        }
        
        if (!config.outputNames.empty()) {
            outputNames_ = config.outputNames;
        } else {
            outputNames_ = {"output"};
        }
        
        initialized_ = true;
        
        LOG_INFO("ONNX Inference Engine initialized successfully");
        LOG_INFO("  Model Path: ", modelPath_);
        LOG_INFO("  Model Type: ", modelType_);
        LOG_INFO("  Input Shape: [", 
                 inputShape_[0], ", ", inputShape_[1], ", ", 
                 inputShape_[2], ", ", inputShape_[3], "]");
        LOG_INFO("  Threshold: ", threshold_);
        
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to initialize ONNX engine: ", e.what());
        return false;
    }
}

std::shared_ptr<InferenceResult> ONNXInferenceEngine::infer(const cv::Mat& inputImage) {
    if (!initialized_) {
        LOG_ERROR("Engine not initialized");
        return nullptr;
    }
    
    if (inputImage.empty()) {
        LOG_ERROR("Empty input image");
        return nullptr;
    }
    
    auto result = std::make_shared<ONNXInferenceResult>();
    auto startTime = std::chrono::high_resolution_clock::now();
    
    try {
        // 预处理图像
        auto preprocessedData = preprocessImage(inputImage);
        
        // 这里应该是真正的ONNX Runtime推理
        // 为了演示，我们创建模拟输出
        std::vector<float> modelOutput;
        
        if (modelType_ == "classification") {
            // 模拟分类输出（1000个类别的概率）
            modelOutput.resize(1000);
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_real_distribution<float> dis(0.0f, 1.0f);
            
            for (auto& val : modelOutput) {
                val = dis(gen);
            }
            
            // 后处理分类结果
            auto classResult = postprocessClassification(modelOutput);
            result->setClassificationResult(classResult);
            
        } else if (modelType_ == "detection") {
            // 模拟检测输出
            modelOutput.resize(25200 * 85); // YOLO格式示例
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_real_distribution<float> dis(0.0f, 1.0f);
            
            for (auto& val : modelOutput) {
                val = dis(gen);
            }
            
            // 后处理检测结果
            auto detections = postprocessDetection(modelOutput, inputImage.size());
            result->setDetectionResults(detections);
            
        } else if (modelType_ == "segmentation") {
            // 模拟分割输出
            int outputHeight = inputImage.rows / 4;
            int outputWidth = inputImage.cols / 4;
            modelOutput.resize(outputHeight * outputWidth);
            
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_real_distribution<float> dis(0.0f, 1.0f);
            
            for (auto& val : modelOutput) {
                val = dis(gen);
            }
            
            // 后处理分割结果
            auto mask = postprocessSegmentation(modelOutput, inputImage.size());
            result->setSegmentationMask(mask);
        }
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
        double inferenceTime = duration.count() / 1000.0; // 转换为毫秒
        
        result->setInferenceTime(inferenceTime);
        result->setValid(true);
        
        return result;
        
    } catch (const std::exception& e) {
        LOG_ERROR("Inference failed: ", e.what());
        return nullptr;
    }
}

std::string ONNXInferenceEngine::getModelInfo() const {
    std::ostringstream oss;
    oss << "ONNX Inference Engine\n";
    oss << "  Model: " << modelPath_ << "\n";
    oss << "  Type: " << modelType_ << "\n";
    oss << "  Input Size: " << inputSize_.width << "x" << inputSize_.height << "\n";
    oss << "  Threshold: " << threshold_ << "\n";
    oss << "  Classes: " << classNames_.size() << "\n";
    oss << "  Initialized: " << (initialized_ ? "Yes" : "No");
    return oss.str();
}

std::vector<float> ONNXInferenceEngine::preprocessImage(const cv::Mat& image) {
    // 调整图像大小
    cv::Mat resized;
    cv::resize(image, resized, inputSize_);
    
    // 转换为RGB（如果是BGR）
    cv::Mat rgb;
    if (image.channels() == 3) {
        cv::cvtColor(resized, rgb, cv::COLOR_BGR2RGB);
    } else {
        rgb = resized;
    }
    
    // 转换为浮点数并归一化
    cv::Mat normalized;
    rgb.convertTo(normalized, CV_32F, 1.0 / 255.0);
    
    // 转换为NCHW格式
    std::vector<float> input_data;
    int total_size = normalized.rows * normalized.cols * normalized.channels();
    input_data.reserve(total_size);
    
    // 分离通道并重排
    std::vector<cv::Mat> channels;
    cv::split(normalized, channels);
    
    for (const auto& channel : channels) {
        float* data = reinterpret_cast<float*>(channel.data);
        input_data.insert(input_data.end(), data, data + channel.rows * channel.cols);
    }
    
    return input_data;
}

ClassificationResult ONNXInferenceEngine::postprocessClassification(const std::vector<float>& output) {
    ClassificationResult result;
    
    // 找到最大值的索引
    auto maxIt = std::max_element(output.begin(), output.end());
    if (maxIt != output.end()) {
        result.classId = static_cast<int>(std::distance(output.begin(), maxIt));
        result.confidence = *maxIt;
        
        // 设置类别名称
        if (result.classId < static_cast<int>(classNames_.size())) {
            result.className = classNames_[result.classId];
        } else {
            result.className = "class_" + std::to_string(result.classId);
        }
    }
    
    return result;
}

std::vector<DetectionBox> ONNXInferenceEngine::postprocessDetection(const std::vector<float>& output, 
                                                                    const cv::Size& imageSize) {
    std::vector<DetectionBox> detections;
    
    // 这里是YOLO格式的简化处理
    // 实际实现需要根据具体的模型格式调整
    int numBoxes = output.size() / 85; // 假设每个框有85个值（4坐标+1置信度+80类别）
    
    for (int i = 0; i < numBoxes && i < 100; ++i) { // 限制最多100个框
        int offset = i * 85;
        
        float confidence = output[offset + 4];
        if (confidence < threshold_) {
            continue;
        }
        
        // 解析边界框
        float x = output[offset + 0] * imageSize.width;
        float y = output[offset + 1] * imageSize.height;
        float w = output[offset + 2] * imageSize.width;
        float h = output[offset + 3] * imageSize.height;
        
        // 找到最大类别概率
        int bestClass = 0;
        float bestScore = 0.0f;
        for (int j = 5; j < 85; ++j) {
            float score = output[offset + j] * confidence;
            if (score > bestScore) {
                bestScore = score;
                bestClass = j - 5;
            }
        }
        
        if (bestScore > threshold_) {
            DetectionBox box;
            box.bbox = cv::Rect2f(x - w/2, y - h/2, w, h);
            box.classId = bestClass;
            box.confidence = bestScore;
            
            if (bestClass < static_cast<int>(classNames_.size())) {
                box.className = classNames_[bestClass];
            } else {
                box.className = "class_" + std::to_string(bestClass);
            }
            
            detections.push_back(box);
        }
    }
    
    // 应用NMS
    return applyNMS(detections);
}

cv::Mat ONNXInferenceEngine::postprocessSegmentation(const std::vector<float>& output, 
                                                    const cv::Size& imageSize) {
    // 假设输出是单通道的分割掩码
    int outputHeight = static_cast<int>(std::sqrt(output.size()));
    int outputWidth = outputHeight;
    
    if (outputHeight * outputWidth != static_cast<int>(output.size())) {
        // 如果不是正方形，尝试推断尺寸
        outputHeight = imageSize.height / 4;
        outputWidth = imageSize.width / 4;
    }
    
    cv::Mat mask(outputHeight, outputWidth, CV_32F);
    std::memcpy(mask.data, output.data(), output.size() * sizeof(float));
    
    // 应用阈值
    cv::Mat binaryMask;
    cv::threshold(mask, binaryMask, threshold_, 255, cv::THRESH_BINARY);
    
    // 调整到原始图像大小
    cv::Mat resizedMask;
    cv::resize(binaryMask, resizedMask, imageSize);
    
    // 转换为8位
    cv::Mat finalMask;
    resizedMask.convertTo(finalMask, CV_8U);
    
    return finalMask;
}

std::vector<DetectionBox> ONNXInferenceEngine::applyNMS(const std::vector<DetectionBox>& boxes) {
    std::vector<DetectionBox> result;
    
    if (boxes.empty()) {
        return result;
    }
    
    // 简化的NMS实现
    const float nmsThreshold = 0.5f;
    
    // 按置信度排序
    std::vector<int> indices;
    for (int i = 0; i < static_cast<int>(boxes.size()); ++i) {
        indices.push_back(i);
    }
    
    std::sort(indices.begin(), indices.end(), [&boxes](int a, int b) {
        return boxes[a].confidence > boxes[b].confidence;
    });
    
    std::vector<bool> suppressed(boxes.size(), false);
    
    for (int i = 0; i < static_cast<int>(indices.size()); ++i) {
        int idx = indices[i];
        if (suppressed[idx]) {
            continue;
        }
        
        result.push_back(boxes[idx]);
        
        // 抑制重叠的框
        for (int j = i + 1; j < static_cast<int>(indices.size()); ++j) {
            int jdx = indices[j];
            if (suppressed[jdx]) {
                continue;
            }
            
            // 计算IoU
            cv::Rect2f rect1 = boxes[idx].bbox;
            cv::Rect2f rect2 = boxes[jdx].bbox;
            
            float intersectionArea = (rect1 & rect2).area();
            float unionArea = rect1.area() + rect2.area() - intersectionArea;
            
            if (unionArea > 0) {
                float iou = intersectionArea / unionArea;
                if (iou > nmsThreshold) {
                    suppressed[jdx] = true;
                }
            }
        }
    }
    
    return result;
}

// InferenceEngineFactory 实现
std::shared_ptr<InferenceEngine> InferenceEngineFactory::createEngine(const std::string& engineType) {
    if (engineType == "onnx") {
        return std::make_shared<ONNXInferenceEngine>();
    }
    // 可以在这里添加其他引擎类型
    
    LOG_ERROR("Unsupported inference engine type: ", engineType);
    return nullptr;
}

} // namespace inference 