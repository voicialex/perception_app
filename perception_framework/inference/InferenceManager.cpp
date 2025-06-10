#include "InferenceManager.hpp"
#include "ONNXInference.hpp"
#include "CVWindow.hpp"
#include <fstream>
#include <sstream>
#include <chrono>
#include <thread>
#include <iomanip>

namespace inference {

InferenceManager& InferenceManager::getInstance() {
    static InferenceManager instance;
    return instance;
}

InferenceManager::~InferenceManager() {
    stop();
}

bool InferenceManager::initialize(const InferenceConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (initialized_) {
        LOG_WARN("InferenceManager already initialized");
        return true;
    }
    
    config_ = config;
    
    if (!config_.isValid()) {
        LOG_ERROR("Invalid inference configuration");
        return false;
    }
    
    // 初始化统计
    stats_.startTime = std::chrono::steady_clock::now();
    
    // 启动异步推理工作线程
    if (config_.asyncInference) {
        shouldStop_ = false;
        asyncWorker_ = std::thread(&InferenceManager::asyncInferenceWorker, this);
        LOG_INFO("Async inference worker thread started");
    }
    
    // 加载默认模型
    if (!config_.defaultModel.empty() && !config_.defaultModelType.empty()) {
        ModelConfig modelConfig;
        modelConfig.modelPath = config_.defaultModel;
        modelConfig.modelType = config_.defaultModelType;
        modelConfig.confidenceThreshold = config_.defaultThreshold;
        
        // 加载类别名称
        if (!config_.classNamesFile.empty()) {
            modelConfig.classNames = loadClassNames(config_.classNamesFile);
        }
        
        if (!loadModel("default", modelConfig)) {
            LOG_WARN("Failed to load default model: ", config_.defaultModel);
        } else {
            LOG_INFO("Default model loaded successfully: ", config_.defaultModel);
        }
    }
    
    initialized_ = true;
    LOG_INFO("InferenceManager initialized successfully");
    return true;
}

bool InferenceManager::loadModel(const std::string& modelName, const ModelConfig& modelConfig) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!modelConfig.isValid()) {
        LOG_ERROR("Invalid model configuration for: ", modelName);
        return false;
    }
    
    // 创建推理引擎
    auto engine = InferenceEngineFactory::createEngine(modelConfig.engineType);
    if (!engine) {
        LOG_ERROR("Failed to create inference engine for: ", modelName);
        return false;
    }
    
    // 初始化引擎
    if (!engine->initialize(modelConfig)) {
        LOG_ERROR("Failed to initialize inference engine for: ", modelName);
        return false;
    }
    
    // 存储引擎
    engines_[modelName] = engine;
    
    LOG_INFO("Model loaded successfully: ", modelName, " (", modelConfig.modelType, ")");
    return true;
}

bool InferenceManager::unloadModel(const std::string& modelName) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = engines_.find(modelName);
    if (it == engines_.end()) {
        LOG_WARN("Model not found: ", modelName);
        return false;
    }
    
    engines_.erase(it);
    LOG_INFO("Model unloaded: ", modelName);
    return true;
}

std::shared_ptr<InferenceResult> InferenceManager::runInference(const std::string& modelName, 
                                                               const cv::Mat& inputImage) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = engines_.find(modelName);
    if (it == engines_.end()) {
        LOG_ERROR("Model not found: ", modelName);
        stats_.failedInferences.fetch_add(1);
        return nullptr;
    }
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    try {
        auto result = it->second->infer(inputImage);
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
        double inferenceTimeMs = duration.count() / 1000.0;
        
        // 更新统计 - 使用fetch_add来处理原子变量
        stats_.totalInferences.fetch_add(1);
        if (result && result->isValid()) {
            stats_.successfulInferences.fetch_add(1);
        } else {
            stats_.failedInferences.fetch_add(1);
        }
        
        // 原子变量不能直接用+=，需要先获取当前值再设置
        double currentTotal = stats_.totalInferenceTime.load();
        stats_.totalInferenceTime.store(currentTotal + inferenceTimeMs);
        
        uint64_t totalCount = stats_.totalInferences.load();
        if (totalCount > 0) {
            stats_.avgInferenceTime.store(stats_.totalInferenceTime.load() / totalCount);
        }
        
        if (config_.enablePerformanceStats) {
            LOG_DEBUG("Inference completed for ", modelName, " in ", inferenceTimeMs, " ms");
        }
        
        return result;
    } catch (const std::exception& e) {
        LOG_ERROR("Inference failed for ", modelName, ": ", e.what());
        stats_.failedInferences.fetch_add(1);
        return nullptr;
    }
}

bool InferenceManager::runInferenceAsync(const std::string& modelName, 
                                        const cv::Mat& inputImage, 
                                        InferenceCallback callback) {
    if (!config_.asyncInference) {
        LOG_ERROR("Async inference is disabled");
        return false;
    }
    
    std::lock_guard<std::mutex> lock(queueMutex_);
    
    if (asyncQueue_.size() >= static_cast<size_t>(config_.maxQueueSize)) {
        LOG_WARN("Async inference queue is full, dropping frame");
        stats_.framesSkipped.fetch_add(1);
        return false;
    }
    
    AsyncTask task;
    task.modelName = modelName;
    task.image = inputImage.clone();
    task.callback = callback ? callback : globalCallback_;
    task.submitTime = std::chrono::steady_clock::now();
    
    asyncQueue_.push(task);
    queueCondition_.notify_one();
    
    return true;
}

bool InferenceManager::processFrame(std::shared_ptr<ob::Frame> frame, OBFrameType frameType) {
    if (!initialized_ || !config_.enableInference) {
        return false;
    }
    
    // 检查帧类型
    if (config_.onlyProcessColorFrames && frameType != OB_FRAME_COLOR) {
        return false;
    }
    
    // 检查处理间隔
    frameCounter_++;
    if (frameCounter_ % config_.inferenceInterval != 0) {
        return false;
    }
    
    // 转换帧为Mat
    cv::Mat image = convertFrameToMat(frame);
    if (image.empty()) {
        LOG_ERROR("Failed to convert frame to Mat");
        return false;
    }
    
    stats_.framesProcessed.fetch_add(1);
    
    // 检查是否有可用的模型
    std::string modelName = "default";
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (engines_.empty()) {
            return false;
        }
        if (engines_.find("default") == engines_.end()) {
            modelName = engines_.begin()->first;
        }
    }
    
    // 执行推理
    if (config_.asyncInference) {
        return runInferenceAsync(modelName, image, globalCallback_);
    } else {
        auto result = runInference(modelName, image);
        if (result && globalCallback_) {
            globalCallback_(modelName, image, result);
        }
        return result != nullptr;
    }
}

cv::Mat InferenceManager::convertFrameToMat(std::shared_ptr<ob::Frame> frame) {
    if (!frame) {
        return cv::Mat();
    }
    
    try {
        // 使用与examples中相同的实现方式
        auto vf = frame->as<ob::VideoFrame>();
        const uint32_t height = vf->getHeight();
        const uint32_t width = vf->getWidth();

        if (frame->getFormat() == OB_FORMAT_BGR) {
            return cv::Mat(height, width, CV_8UC3, frame->getData());
        }
        else if(frame->getFormat() == OB_FORMAT_RGB) {
            // The color channel for RGB images in opencv is BGR, so we need to convert it to BGR before returning the cv::Mat
            auto rgbMat = cv::Mat(height, width, CV_8UC3, frame->getData());
            cv::Mat bgrMat;
            cv::cvtColor(rgbMat, bgrMat, cv::COLOR_RGB2BGR);
            return bgrMat;
        }
        else if(frame->getFormat() == OB_FORMAT_Y16) {
            return cv::Mat(height, width, CV_16UC1, frame->getData());
        }
        
        LOG_WARN("Unsupported frame format for inference: ", static_cast<int>(frame->getFormat()));
        return cv::Mat();
    } catch (const std::exception& e) {
        LOG_ERROR("Error converting frame to Mat: ", e.what());
        return cv::Mat();
    }
}

std::vector<std::string> InferenceManager::loadClassNames(const std::string& filePath) {
    std::vector<std::string> classNames;
    std::ifstream file(filePath);
    
    if (!file.is_open()) {
        LOG_ERROR("Failed to open class names file: ", filePath);
        return classNames;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty()) {
            classNames.push_back(line);
        }
    }
    
    LOG_INFO("Loaded ", classNames.size(), " class names from: ", filePath);
    return classNames;
}

void InferenceManager::asyncInferenceWorker() {
    LOG_INFO("Async inference worker started");
    
    while (!shouldStop_) {
        std::unique_lock<std::mutex> lock(queueMutex_);
        
        // 等待任务或停止信号
        queueCondition_.wait(lock, [this] { 
            return !asyncQueue_.empty() || shouldStop_; 
        });
        
        if (shouldStop_) {
            break;
        }
        
        if (asyncQueue_.empty()) {
            continue;
        }
        
        // 获取任务
        AsyncTask task = asyncQueue_.front();
        asyncQueue_.pop();
        lock.unlock();
        
        // 执行推理
        auto result = runInference(task.modelName, task.image);
        
        // 调用回调
        if (task.callback) {
            try {
                task.callback(task.modelName, task.image, result);
            } catch (const std::exception& e) {
                LOG_ERROR("Error in inference callback: ", e.what());
            }
        }
    }
    
    LOG_INFO("Async inference worker stopped");
}

std::string InferenceManager::getModelInfo(const std::string& modelName) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = engines_.find(modelName);
    if (it == engines_.end()) {
        return "Model not found: " + modelName;
    }
    
    return it->second->getModelInfo();
}

std::vector<std::string> InferenceManager::listModels() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::string> modelNames;
    for (const auto& pair : engines_) {
        modelNames.push_back(pair.first);
    }
    
    return modelNames;
}

bool InferenceManager::setModelThreshold(const std::string& modelName, float threshold) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = engines_.find(modelName);
    if (it == engines_.end()) {
        LOG_ERROR("Model not found: ", modelName);
        return false;
    }
    
    it->second->setThreshold(threshold);
    LOG_INFO("Threshold set for ", modelName, ": ", threshold);
    return true;
}

void InferenceManager::setInferenceCallback(InferenceCallback callback) {
    globalCallback_ = callback;
}

std::string InferenceManager::getStatistics() const {
    std::ostringstream oss;
    oss << "=== Inference Statistics ===\n";
    oss << "Total Inferences: " << stats_.totalInferences.load() << "\n";
    oss << "Successful: " << stats_.successfulInferences.load() << "\n";
    oss << "Failed: " << stats_.failedInferences.load() << "\n";
    oss << "Frames Processed: " << stats_.framesProcessed.load() << "\n";
    oss << "Frames Skipped: " << stats_.framesSkipped.load() << "\n";
    oss << "Average Inference Time: " << std::fixed << std::setprecision(2) 
        << stats_.avgInferenceTime.load() << " ms\n";
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - stats_.startTime).count();
    oss << "Running Time: " << elapsed << " seconds\n";
    
    if (elapsed > 0) {
        double inferenceRate = static_cast<double>(stats_.totalInferences.load()) / elapsed;
        oss << "Inference Rate: " << std::fixed << std::setprecision(2) << inferenceRate << " inferences/sec\n";
    }
    
    oss << "============================";
    return oss.str();
}

void InferenceManager::resetStatistics() {
    stats_.totalInferences = 0;
    stats_.successfulInferences = 0;
    stats_.failedInferences = 0;
    stats_.totalInferenceTime = 0.0;
    stats_.avgInferenceTime = 0.0;
    stats_.framesProcessed = 0;
    stats_.framesSkipped = 0;
    stats_.startTime = std::chrono::steady_clock::now();
    
    LOG_INFO("Inference statistics reset");
}

void InferenceManager::stop() {
    if (!initialized_) {
        return;
    }
    
    shouldStop_ = true;
    
    // 通知异步工作线程停止
    if (asyncWorker_.joinable()) {
        queueCondition_.notify_all();
        asyncWorker_.join();
    }
    
    // 清理引擎
    {
        std::lock_guard<std::mutex> lock(mutex_);
        engines_.clear();
    }
    
    initialized_ = false;
    LOG_INFO("InferenceManager stopped");
}

} // namespace inference 