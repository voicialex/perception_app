#pragma once

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <queue>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <thread>
#include <chrono>
#include <opencv2/opencv.hpp>
#include "libobsensor/ObSensor.hpp"
#include "InferenceBase.hpp"
#include "Logger.hpp"

namespace inference {

/**
 * @brief 推理配置结构，扩展ConfigHelper
 */
struct InferenceConfig {
    bool enableInference = false;           // 是否启用推理
    std::string defaultModel = "";          // 默认模型路径
    std::string defaultModelType = "";      // 默认模型类型
    float defaultThreshold = 0.5f;          // 默认置信度阈值
    bool enableVisualization = true;       // 是否启用可视化
    bool enablePerformanceStats = true;    // 是否启用性能统计
    int inferenceInterval = 1;             // 推理间隔（每N帧推理一次）
    std::string classNamesFile = "";       // 类别名称文件路径
    bool asyncInference = true;            // 是否异步推理
    int maxQueueSize = 10;                 // 异步推理队列最大大小
    bool onlyProcessColorFrames = true;    // 是否只处理彩色帧
    
    bool isValid() const {
        return inferenceInterval > 0 && defaultThreshold >= 0.0f && defaultThreshold <= 1.0f;
    }
};

/**
 * @brief 推理回调函数类型
 */
using InferenceCallback = std::function<void(const std::string& modelName, 
                                           const cv::Mat& image,
                                           std::shared_ptr<InferenceResult> result)>;

/**
 * @brief 推理管理器类，负责管理推理引擎实例和模型加载
 * 与现有的perception_app架构集成
 */
class InferenceManager {
public:
    static InferenceManager& getInstance();
    
    /**
     * @brief 初始化推理管理器
     * @param config 推理配置
     * @return 是否初始化成功
     */
    bool initialize(const InferenceConfig& config = InferenceConfig{});
    
    /**
     * @brief 加载模型
     * @param modelName 模型名称
     * @param modelConfig 模型配置
     * @return 是否加载成功
     */
    bool loadModel(const std::string& modelName, const ModelConfig& modelConfig);
    
    /**
     * @brief 卸载模型
     * @param modelName 模型名称
     * @return 是否卸载成功
     */
    bool unloadModel(const std::string& modelName);
    
    /**
     * @brief 执行推理（同步）
     * @param modelName 模型名称
     * @param inputImage 输入图像
     * @return 推理结果
     */
    std::shared_ptr<InferenceResult> runInference(const std::string& modelName, 
                                                const cv::Mat& inputImage);
    
    /**
     * @brief 执行推理（异步）
     * @param modelName 模型名称
     * @param inputImage 输入图像
     * @param callback 回调函数
     * @return 是否成功提交任务
     */
    bool runInferenceAsync(const std::string& modelName, 
                          const cv::Mat& inputImage, 
                          InferenceCallback callback = nullptr);
    
    /**
     * @brief 处理帧数据（主要接口，在ImageReceiver中调用）
     * @param frame Orbbec帧数据
     * @param frameType 帧类型
     * @return 是否处理成功
     */
    bool processFrame(std::shared_ptr<ob::Frame> frame, OBFrameType frameType);
    
    /**
     * @brief 获取模型信息
     * @param modelName 模型名称
     * @return 模型信息
     */
    std::string getModelInfo(const std::string& modelName) const;
    
    /**
     * @brief 列出所有已加载的模型
     * @return 模型名称列表
     */
    std::vector<std::string> listModels() const;
    
    /**
     * @brief 设置模型阈值
     * @param modelName 模型名称
     * @param threshold 阈值
     * @return 是否设置成功
     */
    bool setModelThreshold(const std::string& modelName, float threshold);
    
    /**
     * @brief 设置推理回调函数
     * @param callback 回调函数
     */
    void setInferenceCallback(InferenceCallback callback);
    
    /**
     * @brief 获取推理统计信息
     * @return 统计信息字符串
     */
    std::string getStatistics() const;
    
    /**
     * @brief 重置统计信息
     */
    void resetStatistics();
    
    /**
     * @brief 检查是否已初始化
     * @return 是否已初始化
     */
    bool isInitialized() const { return initialized_; }
    
    /**
     * @brief 停止推理管理器
     */
    void stop();

private:
    InferenceManager() = default;
    ~InferenceManager();
    InferenceManager(const InferenceManager&) = delete;
    InferenceManager& operator=(const InferenceManager&) = delete;
    
    /**
     * @brief 转换Orbbec帧为OpenCV Mat
     * @param frame Orbbec帧
     * @return OpenCV Mat
     */
    cv::Mat convertFrameToMat(std::shared_ptr<ob::Frame> frame);
    
    /**
     * @brief 加载类别名称文件
     * @param filePath 文件路径
     * @return 类别名称列表
     */
    std::vector<std::string> loadClassNames(const std::string& filePath);
    
    /**
     * @brief 异步推理工作线程
     */
    void asyncInferenceWorker();
    
    /**
     * @brief 异步推理任务结构
     */
    struct AsyncTask {
        std::string modelName;
        cv::Mat image;
        InferenceCallback callback;
        std::chrono::steady_clock::time_point submitTime;
    };

private:
    std::map<std::string, std::shared_ptr<InferenceEngine>> engines_;
    InferenceConfig config_;
    InferenceCallback globalCallback_;
    
    mutable std::mutex mutex_;
    std::atomic<bool> initialized_{false};
    std::atomic<bool> shouldStop_{false};
    
    // 异步推理支持
    std::queue<AsyncTask> asyncQueue_;
    std::mutex queueMutex_;
    std::condition_variable queueCondition_;
    std::thread asyncWorker_;
    
    // 性能统计
    struct Statistics {
        std::atomic<uint64_t> totalInferences{0};
        std::atomic<uint64_t> successfulInferences{0};
        std::atomic<uint64_t> failedInferences{0};
        std::atomic<double> totalInferenceTime{0.0};
        std::atomic<double> avgInferenceTime{0.0};
        std::atomic<uint64_t> framesProcessed{0};
        std::atomic<uint64_t> framesSkipped{0};
        std::chrono::steady_clock::time_point startTime;
    } stats_;
    
    // 帧处理计数器
    std::atomic<uint64_t> frameCounter_{0};
};

} // namespace inference 