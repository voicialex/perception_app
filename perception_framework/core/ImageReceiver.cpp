#include <chrono>
#include <fstream>
#include <iomanip>
#include <map>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>

#include "libobsensor/ObSensor.hpp"
#include "libobsensor/hpp/Error.hpp"
#include "utils.hpp"
#include "utils_opencv.hpp"
#include "MetadataHelper.hpp"
#include "DumpHelper.hpp"
#include "ConfigHelper.hpp"
#include "ImageReceiver.hpp"
#include "DeviceManager.hpp"
#include <opencv2/opencv.hpp>

ImageReceiver::ImageReceiver() : lastFrameTime_(std::chrono::steady_clock::now()) {
    LOG_DEBUG("ImageReceiver created");
}

ImageReceiver::~ImageReceiver() {
    cleanup();
    LOG_DEBUG("ImageReceiver destroyed");
}

bool ImageReceiver::initialize() {
    try {
        LOG_INFO("Initializing ImageReceiver...");
        
        auto& config = ConfigHelper::getInstance();
        
        // 初始化数据保存路径（由DumpHelper负责）
        if(config.saveConfig.enableDump) {
            if (!DumpHelper::getInstance().initializeSavePath()) {
                LOG_WARN("Failed to initialize data save path, data saving may be disabled");
            }
        }
        
        // 配置并行处理
        enableParallelProcessing_ = config.parallelConfig.enableParallelProcessing;
        threadPoolSize_ = config.parallelConfig.threadPoolSize;
        
        // 创建线程池
        if(enableParallelProcessing_) {
            threadPool_ = std::make_unique<utils::ThreadPool>(threadPoolSize_);
            LOG_INFO("Thread pool created, number of threads: ", threadPool_->size());
        } else {
            LOG_INFO("Parallel processing disabled, using serial processing mode");
        }
        
        // 创建设备管理器
        deviceManager_ = std::make_unique<DeviceManager>();
        
        // 设置设备事件回调
        deviceManager_->setDeviceEventCallback([this](DeviceManager::DeviceState oldState, 
                                                      DeviceManager::DeviceState newState, 
                                                      std::shared_ptr<ob::Device> device) {
            onDeviceStateChanged(oldState, newState, device);
        });
        
        // 初始化设备管理器
        if(!deviceManager_->initialize()) {
            LOG_ERROR("Failed to initialize DeviceManager");
            return false;
        }
        
        // 根据配置决定是否创建渲染窗口
        if(config.renderConfig.enableRendering) {
            LOG_INFO("Creating render window...");
        window_ = std::make_unique<ob_smpl::CVWindow>(
            config.renderConfig.windowTitle,
            config.renderConfig.windowWidth,
            config.renderConfig.windowHeight,
            ob_smpl::ARRANGE_GRID
        );
        
        // 设置键盘回调
        setupKeyboardCallbacks();
        
            LOG_DEBUG("Render window created successfully");
        } else {
            LOG_INFO("Rendering disabled, running in headless mode");
        }
        
        // 创建无信号画面（即使在无头模式下也可能需要）
        createNoSignalFrame();
        
        // 如果有窗口，立即显示背景
        if(window_) {
        showNoSignalFrame();
        }
        
        // 启动设备管理器
        deviceManager_->start();
        
        // 如果需要等待设备连接
        if(config.hotPlugConfig.waitForDeviceOnStartup) {
            LOG_INFO("Waiting for device connection...");
            if(!deviceManager_->waitForDevice(10000)) {  // 等待10秒
                LOG_WARN("No device connected within timeout, continuing anyway...");
            }
        }
        
        // 初始化性能统计
        resetPerformanceStats();
        
        isInitialized_ = true;
        LOG_INFO("ImageReceiver initialized successfully");
        return true;
    }
    catch(const std::exception& e) {
        LOG_ERROR("Failed to initialize ImageReceiver: ", e.what());
        return false;
    }
}

// 创建无信号画面
void ImageReceiver::createNoSignalFrame() {
    auto& config = ConfigHelper::getInstance();
    int width = config.renderConfig.windowWidth;
    int height = config.renderConfig.windowHeight;

    // 创建黑色背景
    cv::Mat noSignalMat(height, width, CV_8UC3, cv::Scalar(0, 0, 0)); // 黑色背景 (BGR格式)
    
    // 添加"无信号"文字
    std::string text = "Waiting for signal...";
    int fontFace = cv::FONT_HERSHEY_SIMPLEX;
    double fontScale = 1.0;
    int thickness = 2;
    int baseline = 0;
    
    // 计算文字大小以便居中显示
    cv::Size textSize = cv::getTextSize(text, fontFace, fontScale, thickness, &baseline);
    cv::Point textOrg((width - textSize.width) / 2, (height + textSize.height) / 2);
    
    // 绘制文字
    cv::putText(noSignalMat, text, textOrg, fontFace, fontScale, cv::Scalar(255, 255, 255), thickness);
    
    // 添加时间戳
    auto now = std::chrono::system_clock::now();
    auto now_time = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&now_time), "%Y-%m-%d %H:%M:%S");
    std::string timeStr = ss.str();
    
    cv::putText(noSignalMat, timeStr, cv::Point(10, height - 10), fontFace, 0.5, cv::Scalar(255, 255, 255), 1);
    
    // 直接将Mat保存为成员变量
    noSignalMat_ = noSignalMat.clone();
    
    LOG_DEBUG("No signal frame created with black background");
}

void ImageReceiver::showNoSignalFrame() {
    if(!ConfigHelper::getInstance().renderConfig.enableRendering) {
        // 在无头模式下，只更新内部状态，不进行实际渲染
        showingNoSignalFrame_ = true;
        return;
    }
    
    if(!window_) {
        LOG_DEBUG("No window available for rendering no signal frame");
        return;
    }
    
    // 检查是否需要更新无信号画面（例如更新时间戳）
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastFrameTime_).count();
    
    // 每秒更新一次无信号画面
    if(elapsed >= 1) {
        createNoSignalFrame();
        lastFrameTime_ = now;
    }
    
    // 显示无信号画面
    if(!noSignalMat_.empty()) {
        // 将无信号画面显示在窗口中
        // 创建一个临时的帧向量
        std::vector<std::shared_ptr<const ob::Frame>> emptyFrames;
        window_->pushFramesToView(emptyFrames);
        
        // 使用正确的窗口名称，与视频流使用相同窗口
        auto& config = ConfigHelper::getInstance();
        cv::imshow(config.renderConfig.windowTitle, noSignalMat_);
        cv::waitKey(1);
    }
    
    showingNoSignalFrame_ = true;
}

bool ImageReceiver::isNoSignalFrameShowing() const {
    return showingNoSignalFrame_.load();
}

void ImageReceiver::setupKeyboardCallbacks() {
    if(!window_) {
        LOG_DEBUG("No window available, skipping keyboard callback setup");
        return;
    }
    
    window_->setKeyPressedCallback([this](int key) {
        handleKeyPress(key);
    });
}

void ImageReceiver::handleKeyPress(int key) {
    switch(key) {
        case 27: // ESC键退出
            LOG_INFO("ESC key pressed, exiting...");
            stop();
            break;
        case 'r':
        case 'R': // R键重启设备
            LOG_INFO("R key pressed, rebooting device...");
            deviceManager_->rebootCurrentDevice();
            break;
        case 'p':
        case 'P': // P键打印设备列表
            LOG_INFO("P key pressed, printing device list...");
            deviceManager_->printConnectedDevices();
            break;
        case 's':
        case 'S': // S键显示性能统计
            LOG_INFO("S key pressed, printing performance stats...");
            printPerformanceStats();
            break;
        case 't':
        case 'T': // T键重置性能统计
            LOG_INFO("T key pressed, resetting performance stats...");
            resetPerformanceStats();
            break;
        default:
            // 其他按键忽略，推理和标定相关按键由 PerceptionSystem 处理
            break;
    }
    
    // 打印可用的键盘快捷键
    if (key != -1) { // 只在有按键时显示
        LOG_INFO("Available controls: ESC=Exit, R=Reboot device, P=Print devices, S=Show stats, T=Reset stats");
    }
}

void ImageReceiver::onDeviceStateChanged(DeviceManager::DeviceState oldState, 
                                        DeviceManager::DeviceState newState, 
                                        std::shared_ptr<ob::Device> device) {
    (void)device; // 消除未使用参数警告
    LOG_INFO("Device state changed: ", static_cast<int>(oldState), " -> ", static_cast<int>(newState));
    
    switch(newState) {
        case DeviceManager::DeviceState::CONNECTED:
            LOG_INFO("Device connected, setting up pipelines...");
            // 清理之前的管道状态
            stopPipelines();
            
            if(setupPipelines() && startPipelines()) {
                LOG_INFO("Pipelines started successfully, device ready for streaming");
                showingNoSignalFrame_ = false;
                noFrameCounter_ = 0;
            } else {
                LOG_ERROR("Failed to start pipelines after device connection");
            }
            break;
            
        case DeviceManager::DeviceState::DISCONNECTED:
            LOG_INFO("Device disconnected, stopping pipelines...");
            stopPipelines();
            showingNoSignalFrame_ = false;
            break;
            
        case DeviceManager::DeviceState::RECONNECTING:
            LOG_INFO("Device reconnecting, stopping pipelines...");
            stopPipelines();
            showingNoSignalFrame_ = false;
            break;
            
        case DeviceManager::DeviceState::ERROR:
            LOG_ERROR("Device error occurred, stopping pipelines");
            stopPipelines();
            showingNoSignalFrame_ = false;
            break;
            
        default:
            LOG_DEBUG("Unhandled device state: ", static_cast<int>(newState));
            break;
    }
}

bool ImageReceiver::setupPipelines() {
    auto device = deviceManager_->getCurrentDevice();
    if(!device) {
        LOG_ERROR("No device available for pipeline setup");
        return false;
    }
    
    try {
        auto& config = ConfigHelper::getInstance();
        
        // 重新创建Pipeline对象以确保正确初始化
        mainPipeline_ = std::make_shared<ob::Pipeline>(device);
        
        // 设置主数据流配置
        config_ = std::make_shared<ob::Config>();
        auto sensorList = device->getSensorList();
        
        if(!sensorList) {
            LOG_ERROR("Failed to get sensor list");
            return false;
        }

        // 启用视频传感器
        bool hasEnabledStreams = false;
        for(uint32_t i = 0; i < sensorList->getCount(); ++i) {
            auto sensorType = sensorList->getSensorType(i);
            if(isVideoSensorTypeEnabled(sensorType)) {
                config_->enableStream(sensorType);
                hasEnabledStreams = true;
                LOG_DEBUG("Enabled sensor type: ", sensorType);
            }
        }

        if(!hasEnabledStreams) {
            LOG_WARN("No video streams enabled");
        }

        // 设置IMU数据流
        if(config.streamConfig.enableIMU) {
            imuPipeline_ = std::make_shared<ob::Pipeline>(device);
            imuConfig_ = std::make_shared<ob::Config>();
            imuConfig_->enableGyroStream();
            imuConfig_->enableAccelStream();
            LOG_DEBUG("IMU pipeline configured");
        }

        return true;
    }
    catch(ob::Error &e) {
        handleError(e);
        return false;
    }
}

bool ImageReceiver::startPipelines() {
    if(!mainPipeline_ || !config_) {
        LOG_ERROR("Cannot start pipelines: not properly initialized");
        return false;
    }
    
    try {
        LOG_INFO("Starting pipelines...");
        
        // 启动主数据流
        try {
            mainPipeline_->start(config_, [this](std::shared_ptr<ob::FrameSet> frameset) {
                processFrameSet(frameset);
            });
            LOG_DEBUG("Main pipeline started successfully");
        }
        catch(ob::Error &e) {
            LOG_ERROR("Failed to start main pipeline: ", e.getMessage());
            return false;
        }
        
        // 启动IMU数据流
        if(ConfigHelper::getInstance().streamConfig.enableIMU && 
           imuPipeline_ && imuConfig_) {
            try {
                imuPipeline_->start(imuConfig_, [this](std::shared_ptr<ob::FrameSet> frameset) {
                    std::lock_guard<std::mutex> lock(imuFrameMutex_);
                    for(uint32_t i = 0; i < frameset->frameCount(); ++i) {
                        auto frame = frameset->getFrame(i);
                        imuFrameMap_[frame->type()] = frame;
                        processFrame(frame);
                    }
                });
                LOG_DEBUG("IMU pipeline started successfully");
            }
            catch(ob::Error &e) {
                LOG_WARN("Failed to start IMU pipeline: ", e.getMessage());
                // IMU失败不影响主管道，继续运行
            }
        }
        
        pipelinesRunning_ = true;
        LOG_INFO("All pipelines started successfully");
        return true;
    }
    catch(ob::Error &e) {
        handleError(e);
        return false;
    }
    catch(const std::exception &e) {
        LOG_ERROR("Unexpected error starting pipelines: ", e.what());
        return false;
    }
}

void ImageReceiver::processFrameSet(std::shared_ptr<ob::FrameSet> frameset) {
    if(!frameset) return;

    // Update performance statistics
    performanceStats_.frameCount++;
    performanceStats_.totalFrames++;

    if(enableParallelProcessing_ && threadPool_) {
        // Use parallel processing
        processFrameSetParallel(frameset);
    } else {
        // Use serial processing
    std::unique_lock<std::mutex> lk(frameMutex_);
    for(uint32_t i = 0; i < frameset->frameCount(); ++i) {
        auto frame = frameset->getFrame(i);
            if(frame) {
        frameMap_[frame->type()] = frame;
        processFrame(frame);
            }
        }
    }
}

void ImageReceiver::processFrameSetParallel(std::shared_ptr<ob::FrameSet> frameset) {
    if(!frameset || !threadPool_) return;
    
    // Clean up completed tasks
    cleanupCompletedTasks();
    
    // Get the number of frames in the frame set
    uint32_t frameCount = frameset->frameCount();

    // Log frame set information
    LOG_DEBUG("Received frame set, frame count: ", frameCount, ", timestamp: ", frameset->timeStamp());
    
    // Get all frames and process in parallel
    for(uint32_t i = 0; i < frameCount; ++i) {
        auto frame = frameset->getFrame(i);
        
        if(!frame) {
            LOG_WARN("Received empty frame, index: ", i);
            continue;
        }
        
        // Record frame type
        OBFrameType frameType = frame->type();
        
        // Save frame reference to frame map
        {
            std::unique_lock<std::mutex> lk(frameMutex_);
            frameMap_[frameType] = frame;
        }
        
        // Submit processing task to thread pool
        try {
            std::future<void> future = threadPool_->enqueue(
                [this, frame]() {
                    // 记录处理开始时间
                    auto start = std::chrono::steady_clock::now();
                    // 处理帧
                    this->processFrame(frame);
                    
                    // 计算处理时间
                    auto end = std::chrono::steady_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

                    // 更新性能统计数据
                    performanceStats_.currentProcessingTime = duration;
                    performanceStats_.totalProcessingTime += duration;
                    performanceStats_.processedFramesCount++;
                    
                    // 更新最小和最大处理时间
                    // 注意：这里可能有多线程竞争，但对统计数据影响很小
                    if (duration < performanceStats_.minProcessingTime) {
                        performanceStats_.minProcessingTime = duration;
                    }
                    if (duration > performanceStats_.maxProcessingTime) {
                        performanceStats_.maxProcessingTime = duration;
                    }

                    // 帧类型文本描述
                    std::string frameTypeStr = ob::TypeHelper::convertOBFrameTypeToString(frame->type());

                    LOG_DEBUG("Frame processed, type: ", frameTypeStr, " (", static_cast<int>(frame->type()), ")", 
                              ", index: ", frame->index(), 
                              ", timestamp: ", frame->timeStamp(),
                              ", duration: ", duration, " ms");
                }
            );
            
            // Save future to track task completion
            std::unique_lock<std::mutex> lk(futuresMutex_);
            frameFutures_.push_back(std::move(future));
        } 
        catch(const std::exception& e) {
            LOG_ERROR("Failed to submit task to thread pool: ", e.what());
            // If submission fails, process in current thread
            processFrame(frame);
        }
    }
    
    // Log task queue status
    {
        std::unique_lock<std::mutex> lk(futuresMutex_);
        LOG_DEBUG("Current pending tasks: ", frameFutures_.size(), 
                  ", thread pool queue size: ", threadPool_->queueSize());
    }
}

void ImageReceiver::cleanupCompletedTasks() {
    std::unique_lock<std::mutex> lk(futuresMutex_);
    
    // Remove completed tasks
    frameFutures_.erase(
        std::remove_if(
            frameFutures_.begin(), 
            frameFutures_.end(),
            [](std::future<void>& f) {
                // Check if task is completed
                return f.valid() && 
                       f.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready;
            }
        ),
        frameFutures_.end()
    );
    
    // If queue is too long, wait for some tasks to complete
    auto& config = ConfigHelper::getInstance();
    if(frameFutures_.size() > static_cast<size_t>(config.parallelConfig.maxQueuedTasks)) {
        LOG_WARN("Waiting for tasks in queue to complete, current queue size: ", frameFutures_.size());
        if(!frameFutures_.empty()) {
            // Wait for first task to complete
            frameFutures_[0].wait();
        }
    }
}

void ImageReceiver::processFrame(std::shared_ptr<ob::Frame> frame) {
    if(!frame) return;

    auto& config = ConfigHelper::getInstance();
    auto& metadataHelper = MetadataHelper::getInstance();
    auto& frameHelper = DumpHelper::getInstance();

    // Process metadata
    if(config.metadataConfig.enableMetadata && 
       frame->index() % config.metadataConfig.printInterval == 0) {
        metadataHelper.printMetadata(frame, config.metadataConfig.printInterval);
    }

    // Process frame saving - based on configured frame interval
    if(config.saveConfig.enableDump) {
        // Use simplified frame interval control logic
        if(frame->index() % config.saveConfig.frameInterval == 0) {
            // 帧类型文本描述
            std::string frameTypeStr = ob::TypeHelper::convertOBFrameTypeToString(frame->type());
            
            LOG_DEBUG("Saving frame, type: ", frameTypeStr, " (", static_cast<int>(frame->type()), ")", 
                      ", index: ", frame->index());
            frameHelper.save(frame, config.saveConfig.dumpPath);
        }
    }
    
    // 调用帧处理回调（通知 PerceptionSystem）
    if (frameProcessCallback_) {
        try {
            frameProcessCallback_(frame, frame->type());
        } catch (const std::exception& e) {
            LOG_ERROR("Frame process callback failed: ", e.what());
        }
    }
}

void ImageReceiver::run() {
    if(!isInitialized_) {
        LOG_ERROR("ImageReceiver not initialized, cannot run");
        return;
    }
    
    try {
        LOG_INFO("Starting ImageReceiver main loop...");
        
        auto& config = ConfigHelper::getInstance();
        
        // 如果启用渲染，初始化时先显示无信号画面
        if(config.renderConfig.enableRendering && window_) {
            showNoSignalFrame();
        }
        
        // 主循环
        while(!shouldExit_) {
            // 只在有窗口时检查窗口状态
            if(config.renderConfig.enableRendering && window_) {
            if(!window_->run()) {
                LOG_INFO("Window closed, exiting...");
                break;
                }
            }
            
            renderFrames();
            updatePerformanceStats();
            
            // 控制循环频率
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        LOG_INFO("Main loop exited");
        cleanup();
    }
    catch(ob::Error &e) {
        handleError(e);
        cleanup();
    }
    catch(const std::exception &e) {
        LOG_ERROR("Runtime error: ", e.what());
        cleanup();
                }
}

void ImageReceiver::renderFrames() {
    auto& config = ConfigHelper::getInstance();
    
    if(!config.renderConfig.enableRendering || !window_) {
        // 在无头模式下或无窗口时，仍然可以处理数据但不进行渲染
        // 保持帧数据更新以支持其他功能（如保存、推理等）
        return;
    }
    
    std::vector<std::shared_ptr<const ob::Frame>> framesForRender;

    // 收集主数据流帧
    {
        std::lock_guard<std::mutex> lock(frameMutex_);
        for(auto &frame: frameMap_) {
            framesForRender.push_back(std::const_pointer_cast<const ob::Frame>(frame.second));
        }
    }

    // 收集IMU数据流帧
    if(config.streamConfig.enableIMU) {
        std::lock_guard<std::mutex> lock(imuFrameMutex_);
        for(auto &frame: imuFrameMap_) {
            framesForRender.push_back(std::const_pointer_cast<const ob::Frame>(frame.second));
        }
    }

    // 渲染帧
    auto deviceState = deviceManager_->getDeviceState();
    if(deviceState == DeviceManager::DeviceState::CONNECTED) {
        if(!framesForRender.empty()) {
            // 有帧数据，正常显示
            window_->pushFramesToView(framesForRender);
            showingNoSignalFrame_ = false;
            noFrameCounter_ = 0;
            lastFrameTime_ = std::chrono::steady_clock::now();
        } else {
            // 设备已连接但没有帧数据
            noFrameCounter_++;
            
            // 如果超过一定时间没有帧数据，显示无信号画面
            // 这里设置为30帧（约3秒）后显示无信号画面
            if(noFrameCounter_ > 30) {
                showNoSignalFrame();
            }
            
            if(noFrameCounter_ % 100 == 0) {  // 每100次循环打印一次
                LOG_DEBUG("Device connected but no frames available yet (count: ", noFrameCounter_, ")");
            }
        }
    } else {
        static int waitCount = 0;
        if(++waitCount % 100 == 0) {  // 每100次循环打印一次
            // 设备未连接，显示无信号画面
            showNoSignalFrame();
            LOG_DEBUG("Waiting for device connection... (state: ", static_cast<int>(deviceState), ")");
        }
    }
}

void ImageReceiver::updatePerformanceStats() {
    auto& config = ConfigHelper::getInstance().inferenceConfig;
    if(!config.enablePerformanceStats) {
        return;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - performanceStats_.lastStatsTime).count();
    
    if(elapsed >= 1000) {  // 每秒更新一次
        auto totalElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - performanceStats_.startTime).count();
        
        performanceStats_.currentFPS = (performanceStats_.frameCount.load() * 1000.0) / elapsed;
        performanceStats_.averageFPS = (performanceStats_.totalFrames.load() * 1000.0) / totalElapsed;
        
        // 计算平均处理时间
        auto processedFrames = performanceStats_.processedFramesCount.load();
        if (processedFrames > 0) {
            performanceStats_.avgProcessingTime = 
                static_cast<double>(performanceStats_.totalProcessingTime.load()) / processedFrames;
        }
        
        // 重置帧计数器，但保留总处理时间统计(仅在需要时重置)
        performanceStats_.frameCount = 0;
        performanceStats_.lastStatsTime = now;
        
        updateWindowTitle();
    }
}

void ImageReceiver::updateWindowTitle() {
    auto& config = ConfigHelper::getInstance().renderConfig;
    if(!config.showFPS || !window_) {
        return;
    }
    
    std::string title = config.windowTitle;
    if(performanceStats_.currentFPS > 0) {
        title += " - FPS: " + std::to_string(static_cast<int>(performanceStats_.currentFPS));
    }
    
    // 添加当前处理时间到窗口标题
    if(performanceStats_.currentProcessingTime > 0) {
        title += " - Processing: " + std::to_string(static_cast<int>(performanceStats_.currentProcessingTime.load())) + " ms";
    }
    
    // 注意：这里需要CVWindow支持动态标题更新，如果不支持则跳过
    // window_->setTitle(title);
}

void ImageReceiver::printPerformanceStats() {
    LOG_INFO("=== Performance Statistics ===");
    LOG_INFO("Current FPS: ", performanceStats_.currentFPS);
    LOG_INFO("Average FPS: ", performanceStats_.averageFPS);
    LOG_INFO("Total Frames: ", performanceStats_.totalFrames.load());
    
    // 添加处理时间统计信息
    LOG_INFO("Frame Processing Time Statistics:");
    LOG_INFO("  - Current: ", performanceStats_.currentProcessingTime.load(), " ms");
    LOG_INFO("  - Average: ", performanceStats_.avgProcessingTime, " ms");
    LOG_INFO("  - Minimum: ", performanceStats_.minProcessingTime, " ms");
    LOG_INFO("  - Maximum: ", performanceStats_.maxProcessingTime, " ms");
    LOG_INFO("  - Processed Frames: ", performanceStats_.processedFramesCount.load());
    
    LOG_INFO("Device State: ", static_cast<int>(deviceManager_->getDeviceState()));
    LOG_INFO("Pipelines Running: ", pipelinesRunning_.load());
    
    // 添加线程池统计
    if(enableParallelProcessing_ && threadPool_) {
        std::unique_lock<std::mutex> lk(futuresMutex_);
        LOG_INFO("Thread Pool Size: ", threadPool_->size());
        LOG_INFO("Pending Tasks: ", frameFutures_.size());
        LOG_INFO("Thread Pool Queue Size: ", threadPool_->queueSize());
    } else {
        LOG_INFO("Parallel Processing: Disabled");
    }
    
    LOG_INFO("==============================");
    }

void ImageReceiver::stop() {
    LOG_INFO("Stopping ImageReceiver...");
    shouldExit_ = true;
}

bool ImageReceiver::isVideoSensorTypeEnabled(OBSensorType sensorType) {
    // 首先使用 SDK 提供的方法判断是否为视频传感器
    if (!ob::TypeHelper::isVideoSensorType(sensorType)) {
        return false;
    }
    
    // 再根据配置判断是否启用
    auto& config = ConfigHelper::getInstance().streamConfig;
    switch(sensorType) {
        case OB_SENSOR_COLOR: return config.enableColor;
        case OB_SENSOR_DEPTH: return config.enableDepth;
        case OB_SENSOR_IR: return config.enableIR;
        case OB_SENSOR_IR_LEFT: return config.enableIRLeft;
        case OB_SENSOR_IR_RIGHT: return config.enableIRRight;
        default: return false;
    }
}

void ImageReceiver::handleError(ob::Error &e) {
    LOG_ERROR("Orbbec SDK Error: function=", e.getName(), 
              ", args=", e.getArgs(), 
              ", message=", e.getMessage(), 
              ", type=", e.getExceptionType());
}

void ImageReceiver::cleanup() {
    try {
        LOG_INFO("Cleaning up ImageReceiver...");
        
        shouldExit_ = true;
        stopPipelines();
        
        // 等待所有任务完成
        if(threadPool_) {
            LOG_INFO("Waiting for thread pool tasks to complete...");
            cleanupCompletedTasks();
        }
        
        // 清理线程池
        threadPool_.reset();
        
        // 清理无信号帧
        noSignalMat_.release();
        
        // 停止设备管理器
        if(deviceManager_) {
            deviceManager_->stop();
            deviceManager_.reset();
        }
        
        // 清理配置对象
        config_.reset();
        imuConfig_.reset();
        imuPipeline_.reset();
        mainPipeline_.reset();
        
        // 清理窗口
        if(window_) {
        window_.reset();
            LOG_DEBUG("Window cleaned up");
        }
        
        isInitialized_ = false;
        
        LOG_INFO("Cleanup completed");
    }
    catch(const std::exception &e) {
        LOG_ERROR("Error in cleanup: ", e.what());
    }
}

void ImageReceiver::stopPipelines() {
    try {
        LOG_INFO("Stopping pipelines...");
        
        pipelinesRunning_ = false;
        
        // 停止主数据流
        if(mainPipeline_) {
            try {
                mainPipeline_->stop();
                LOG_DEBUG("Main pipeline stopped");
            }
            catch(ob::Error &e) {
                LOG_WARN("Error stopping main pipeline: ", e.getMessage());
            }
        }
        
        // 停止IMU数据流
        if(imuPipeline_) {
            try {
            imuPipeline_->stop();
                LOG_DEBUG("IMU pipeline stopped");
            }
            catch(ob::Error &e) {
                LOG_WARN("Error stopping IMU pipeline: ", e.getMessage());
            }
        }

        // 清理帧数据
        {
            std::lock_guard<std::mutex> lock(frameMutex_);
            frameMap_.clear();
        }
        
        {
            std::lock_guard<std::mutex> lock(imuFrameMutex_);
            imuFrameMap_.clear();
        }

        LOG_INFO("All pipelines stopped");
    }
    catch(const std::exception &e) {
        LOG_ERROR("Error stopping pipelines: ", e.what());
    }
}

bool ImageReceiver::startStreaming() {
    LOG_INFO("Starting streaming...");
    
    if (streamState_ == StreamState::RUNNING) {
        LOG_WARN("Streaming already running");
        return true;
    }
    
    try {
        // 设置管道
        if (!setupPipelines()) {
            LOG_ERROR("Failed to setup pipelines");
            streamState_ = StreamState::ERROR;
            return false;
        }
    
    // 启动管道
    if (!startPipelines()) {
        LOG_ERROR("Failed to start pipelines");
            streamState_ = StreamState::ERROR;
            return false;
        }
        
        // 更新状态
        streamState_ = StreamState::RUNNING;
        LOG_INFO("Streaming started successfully");
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR("Failed to start streaming: ", e.what());
        streamState_ = StreamState::ERROR;
        return false;
    }
}

void ImageReceiver::stopStreaming() {
    LOG_INFO("Stopping streaming...");
    
    if (streamState_ == StreamState::IDLE) {
        LOG_DEBUG("Streaming already stopped");
        return;
    }
    
    try {
    // 停止管道
    stopPipelines();
    
        // 更新状态
        streamState_ = StreamState::IDLE;
        LOG_INFO("Streaming stopped successfully");
    }
    catch (const std::exception& e) {
        LOG_ERROR("Error stopping streaming: ", e.what());
        streamState_ = StreamState::ERROR;
    }
}

bool ImageReceiver::restartStreaming() {
    LOG_INFO("Restarting streaming...");
    
    // 先停止
    stopStreaming();
    
    // 短暂延迟确保资源释放
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 重新启动
    return startStreaming();
}

void ImageReceiver::resetPerformanceStats() {
    auto now = std::chrono::steady_clock::now();
    
    performanceStats_.frameCount = 0;
    performanceStats_.totalFrames = 0;
    performanceStats_.startTime = now;
    performanceStats_.lastStatsTime = now;
    performanceStats_.currentFPS = 0.0;
    performanceStats_.averageFPS = 0.0;
    
    // 重置处理时间统计
    performanceStats_.totalProcessingTime = 0;
    performanceStats_.processedFramesCount = 0;
    performanceStats_.avgProcessingTime = 0.0;
    performanceStats_.minProcessingTime = std::numeric_limits<double>::max();
    performanceStats_.maxProcessingTime = 0.0;
    performanceStats_.currentProcessingTime = 0.0;
    
    LOG_DEBUG("Performance statistics reset");
}
