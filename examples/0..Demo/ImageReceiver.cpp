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

ImageReceiver::ImageReceiver() {
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
        
        // 创建渲染窗口
        window_ = std::make_unique<ob_smpl::CVWindow>(
            config.renderConfig.windowTitle,
            config.renderConfig.windowWidth,
            config.renderConfig.windowHeight,
            ob_smpl::ARRANGE_GRID
        );
        
        // 设置键盘回调
        setupKeyboardCallbacks();
        
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
        performanceStats_.startTime = std::chrono::steady_clock::now();
        performanceStats_.lastStatsTime = performanceStats_.startTime;
        
        isInitialized_ = true;
        LOG_INFO("ImageReceiver initialized successfully");
        return true;
    }
    catch(const std::exception& e) {
        LOG_ERROR("Failed to initialize ImageReceiver: ", e.what());
        return false;
    }
}

void ImageReceiver::setupKeyboardCallbacks() {
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
            printPerformanceStats();
            break;
        default:
            break;
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
            } else {
                LOG_ERROR("Failed to start pipelines after device connection");
            }
            break;
            
        case DeviceManager::DeviceState::DISCONNECTED:
            LOG_INFO("Device disconnected, stopping pipelines...");
            stopPipelines();
            break;
            
        case DeviceManager::DeviceState::RECONNECTING:
            LOG_INFO("Device reconnecting, stopping pipelines...");
            stopPipelines();
            break;
            
        case DeviceManager::DeviceState::ERROR:
            LOG_ERROR("Device error occurred, stopping pipelines");
            stopPipelines();
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

    // 更新性能统计
    performanceStats_.frameCount++;
    performanceStats_.totalFrames++;

    std::unique_lock<std::mutex> lk(frameMutex_);
    for(uint32_t i = 0; i < frameset->frameCount(); ++i) {
        auto frame = frameset->getFrame(i);
        frameMap_[frame->type()] = frame;
        processFrame(frame);
    }
}

void ImageReceiver::processFrame(std::shared_ptr<ob::Frame> frame) {
    if(!frame) return;

    auto& config = ConfigHelper::getInstance();
    auto& metadataHelper = MetadataHelper::getInstance();
    auto& frameHelper = DumpHelper::getInstance();

    if(config.metadataConfig.enableMetadata && 
       frame->index() % config.metadataConfig.printInterval == 0) {
        metadataHelper.printMetadata(frame, config.metadataConfig.printInterval);
    }

    if(config.saveConfig.enableDump) {
        frameHelper.saveFrame(frame, config.saveConfig.dumpPath);
    }
}

void ImageReceiver::run() {
    if(!isInitialized_) {
        LOG_ERROR("ImageReceiver not initialized, cannot run");
        return;
    }
    
    try {
        LOG_INFO("Starting ImageReceiver main loop...");
        LOG_INFO("Controls: ESC-Exit, R-Reboot, P-Print devices, S-Stats");
        
        // 主循环 - 优先检查shouldExit_
        while(!shouldExit_) {
            // 检查窗口状态，如果窗口关闭也退出
            if(!window_->run()) {
                LOG_INFO("Window closed, exiting...");
                break;
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
    if(!ConfigHelper::getInstance().renderConfig.enableRendering) {
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
    if(ConfigHelper::getInstance().streamConfig.enableIMU) {
        std::lock_guard<std::mutex> lock(imuFrameMutex_);
        for(auto &frame: imuFrameMap_) {
            framesForRender.push_back(std::const_pointer_cast<const ob::Frame>(frame.second));
        }
    }

    // 渲染帧
    auto deviceState = deviceManager_->getDeviceState();
    if(deviceState == DeviceManager::DeviceState::CONNECTED) {
        if(!framesForRender.empty()) {
            window_->pushFramesToView(framesForRender);
        } else {
            // 设备已连接但没有帧数据，可能是管道刚启动
            static int noFrameCount = 0;
            if(++noFrameCount % 100 == 0) {  // 每100次循环打印一次
                LOG_DEBUG("Device connected but no frames available yet (count: ", noFrameCount, ")");
            }
        }
    } else {
        // 设备未连接，显示等待状态
        static int waitCount = 0;
        if(++waitCount % 100 == 0) {  // 每100次循环打印一次
            LOG_DEBUG("Waiting for device connection... (state: ", static_cast<int>(deviceState), ")");
        }
    }
}

void ImageReceiver::updatePerformanceStats() {
    auto& config = ConfigHelper::getInstance().debugConfig;
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
        
        performanceStats_.frameCount = 0;
        performanceStats_.lastStatsTime = now;
        
        updateWindowTitle();
    }
}

void ImageReceiver::updateWindowTitle() {
    auto& config = ConfigHelper::getInstance().renderConfig;
    if(!config.showFPS) {
        return;
    }
    
    std::string title = config.windowTitle;
    if(performanceStats_.currentFPS > 0) {
        title += " - FPS: " + std::to_string(static_cast<int>(performanceStats_.currentFPS));
    }
    
    // 注意：这里需要CVWindow支持动态标题更新，如果不支持则跳过
    // window_->setTitle(title);
}

void ImageReceiver::printPerformanceStats() {
    LOG_INFO("=== Performance Statistics ===");
    LOG_INFO("Current FPS: ", performanceStats_.currentFPS);
    LOG_INFO("Average FPS: ", performanceStats_.averageFPS);
    LOG_INFO("Total Frames: ", performanceStats_.totalFrames.load());
    LOG_INFO("Device State: ", static_cast<int>(deviceManager_->getDeviceState()));
    LOG_INFO("Pipelines Running: ", pipelinesRunning_.load());
    LOG_INFO("==============================");
}

void ImageReceiver::stop() {
    LOG_INFO("Stopping ImageReceiver...");
    shouldExit_ = true;
}

bool ImageReceiver::isVideoSensorTypeEnabled(OBSensorType sensorType) {
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
        window_.reset();
        
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
