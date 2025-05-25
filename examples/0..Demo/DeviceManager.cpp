#include "DeviceManager.hpp"
#include <iomanip>

DeviceManager::DeviceManager() 
    : context_(std::make_shared<ob::Context>()) {
    LOG_DEBUG("DeviceManager created");
}

DeviceManager::~DeviceManager() {
    stop();
    LOG_DEBUG("DeviceManager destroyed");
}

bool DeviceManager::initialize() {
    try {
        LOG_INFO("Initializing DeviceManager...");
        
        auto& config = ConfigHelper::getInstance().hotPlugConfig;
        
        if(config.enableHotPlug) {
            setupHotPlugCallback();
        }
        
        // 尝试连接设备
        if(attemptConnection()) {
            setDeviceState(DeviceState::CONNECTED);
            LOG_INFO("Device connected during initialization");
        } else {
            setDeviceState(DeviceState::DISCONNECTED);
            if(config.waitForDeviceOnStartup) {
                LOG_INFO("No device found, will wait for device connection...");
            } else {
                LOG_WARN("No device found and waitForDeviceOnStartup is disabled");
            }
        }
        
        return true;
    }
    catch(ob::Error &e) {
        LOG_ERROR("Failed to initialize DeviceManager: ", e.getMessage());
        setDeviceState(DeviceState::ERROR);
        return false;
    }
}

void DeviceManager::start() {
    LOG_INFO("Starting DeviceManager");
    shouldStop_ = false;
    
    // 启动重连线程
    if(!reconnectionThread_.joinable()) {
        reconnectionThread_ = std::thread(&DeviceManager::reconnectionWorker, this);
    }
}

void DeviceManager::stop() {
    LOG_INFO("Stopping DeviceManager");
    shouldStop_ = true;
    isReconnecting_ = false;  // 立即停止重连
    
    // 通知所有等待的线程
    deviceCondition_.notify_all();
    
    // 等待重连线程结束，但设置超时避免无限等待
    if(reconnectionThread_.joinable()) {
        auto start = std::chrono::steady_clock::now();
        while(reconnectionThread_.joinable()) {
            deviceCondition_.notify_all();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            
            // 超时检查，避免无限等待
            auto elapsed = std::chrono::steady_clock::now() - start;
            if(elapsed > std::chrono::seconds(2)) {
                LOG_WARN("Reconnection thread join timeout, detaching...");
                reconnectionThread_.detach();
                break;
            }
            
            if(!reconnectionThread_.joinable()) {
                break;
            }
            
            // 尝试join
            try {
                reconnectionThread_.join();
                break;
            } catch(...) {
                // join失败，继续尝试
            }
        }
    }
    
    std::lock_guard<std::mutex> lock(deviceMutex_);
    currentDevice_.reset();
    setDeviceState(DeviceState::DISCONNECTED);
}

std::shared_ptr<ob::Device> DeviceManager::getCurrentDevice() const {
    std::lock_guard<std::mutex> lock(deviceMutex_);
    return currentDevice_;
}

bool DeviceManager::rebootCurrentDevice() {
    std::lock_guard<std::mutex> lock(deviceMutex_);
    
    if(!currentDevice_) {
        LOG_WARN("No device connected to reboot");
        return false;
    }
    
    try {
        LOG_INFO("Rebooting device...");
        currentDevice_->reboot();
        return true;
    }
    catch(ob::Error &e) {
        LOG_ERROR("Failed to reboot device: ", e.getMessage());
        return false;
    }
}

void DeviceManager::printConnectedDevices() {
    auto deviceList = context_->queryDeviceList();
    printDeviceList("connected", deviceList);
}

bool DeviceManager::waitForDevice(int timeoutMs) {
    std::unique_lock<std::mutex> lock(deviceMutex_);
    
    if(deviceState_ == DeviceState::CONNECTED) {
        return true;
    }
    
    if(timeoutMs == 0) {
        deviceCondition_.wait(lock, [this] { 
            return deviceState_ == DeviceState::CONNECTED || shouldStop_; 
        });
    } else {
        auto timeout = std::chrono::milliseconds(timeoutMs);
        deviceCondition_.wait_for(lock, timeout, [this] { 
            return deviceState_ == DeviceState::CONNECTED || shouldStop_; 
        });
    }
    
    return deviceState_ == DeviceState::CONNECTED;
}

void DeviceManager::setDeviceState(DeviceState newState) {
    DeviceState oldState = deviceState_.exchange(newState);
    
    if(oldState != newState) {
        LOG_DEBUG("Device state changed: ", static_cast<int>(oldState), " -> ", static_cast<int>(newState));
        
        if(deviceEventCallback_) {
            deviceEventCallback_(oldState, newState, getCurrentDevice());
        }
        
        if(newState == DeviceState::CONNECTED) {
            deviceCondition_.notify_all();
        }
    }
}

void DeviceManager::setupHotPlugCallback() {
    context_->setDeviceChangedCallback([this](std::shared_ptr<ob::DeviceList> removedList, 
                                              std::shared_ptr<ob::DeviceList> addedList) {
        onDeviceChanged(removedList, addedList);
    });
    LOG_DEBUG("Hot plug callback registered");
}

void DeviceManager::onDeviceChanged(std::shared_ptr<ob::DeviceList> removedList, 
                                   std::shared_ptr<ob::DeviceList> addedList) {
    auto& config = ConfigHelper::getInstance().hotPlugConfig;
    
    if(config.printDeviceEvents) {
        printDeviceList("removed", removedList);
        printDeviceList("added", addedList);
    }
    
    // 使用异步处理避免在回调中阻塞
    std::thread([this, removedList, addedList]() {
        auto& config = ConfigHelper::getInstance().hotPlugConfig;
        
        // 处理设备断开
        if(removedList && removedList->getCount() > 0) {
            handleDeviceDisconnection();
        }
        
        // 处理设备连接
        if(addedList && addedList->getCount() > 0) {
            // 等待一段时间确保设备稳定
            std::this_thread::sleep_for(std::chrono::milliseconds(config.deviceStabilizeDelayMs));
            handleDeviceConnection();
        }
    }).detach();
}

void DeviceManager::handleDeviceDisconnection() {
    LOG_INFO("Device disconnected");
    
    {
        std::lock_guard<std::mutex> lock(deviceMutex_);
        currentDevice_.reset();
        lastDisconnectTime_ = std::chrono::steady_clock::now();
    }
    
    setDeviceState(DeviceState::DISCONNECTED);
    
    // 如果启用自动重连，开始重连
    if(ConfigHelper::getInstance().hotPlugConfig.autoReconnect) {
        isReconnecting_ = true;
        reconnectAttempts_ = 0;
        deviceCondition_.notify_all();
    }
}

void DeviceManager::handleDeviceConnection() {
    LOG_INFO("New device detected, attempting to connect...");
    
    if(attemptConnection()) {
        setDeviceState(DeviceState::CONNECTED);
        isReconnecting_ = false;
        reconnectAttempts_ = 0;
        LOG_INFO("Device connected successfully");
    } else {
        LOG_WARN("Failed to connect to new device");
    }
}

bool DeviceManager::attemptConnection() {
    try {
        auto deviceList = context_->queryDeviceList();
        if(deviceList->getCount() == 0) {
            return false;
        }
        
        std::lock_guard<std::mutex> lock(deviceMutex_);
        currentDevice_ = deviceList->getDevice(0);
        
        if(currentDevice_) {
            LOG_INFO("Device connected successfully");
            printDeviceList("connected", deviceList);
            return true;
        }
        
        return false;
    }
    catch(ob::Error &e) {
        LOG_ERROR("Failed to connect device: ", e.getMessage());
        return false;
    }
}

void DeviceManager::reconnectionWorker() {
    LOG_DEBUG("Reconnection worker started");
    
    while(!shouldStop_) {
        std::unique_lock<std::mutex> lock(deviceMutex_);
        
        // 等待重连信号或停止信号，设置超时避免无限等待
        deviceCondition_.wait_for(lock, std::chrono::milliseconds(100), [this] { 
            return isReconnecting_ || shouldStop_; 
        });
        
        if(shouldStop_) break;
        
        if(isReconnecting_ && !shouldStop_) {
            lock.unlock();
            
            auto& config = ConfigHelper::getInstance().hotPlugConfig;
            
            while(isReconnecting_ && reconnectAttempts_ < config.maxReconnectAttempts && !shouldStop_) {
                reconnectAttempts_++;
                setDeviceState(DeviceState::RECONNECTING);
                
                LOG_INFO("Reconnection attempt ", reconnectAttempts_.load(), "/", config.maxReconnectAttempts);
                
                // 分段等待重连延迟，以便快速响应停止信号
                int delayMs = config.reconnectDelayMs;
                while(delayMs > 0 && !shouldStop_) {
                    int sleepTime = std::min(delayMs, 100);  // 每次最多睡眠100ms
                    std::this_thread::sleep_for(std::chrono::milliseconds(sleepTime));
                    delayMs -= sleepTime;
                }
                
                if(shouldStop_) break;
                
                if(attemptConnection()) {
                    setDeviceState(DeviceState::CONNECTED);
                    isReconnecting_ = false;
                    reconnectAttempts_ = 0;
                    LOG_INFO("Reconnection successful on attempt ", reconnectAttempts_.load());
                    break;
                }
            }
            
            if(isReconnecting_ && reconnectAttempts_ >= config.maxReconnectAttempts) {
                LOG_ERROR("Reconnection failed after ", config.maxReconnectAttempts, " attempts");
                setDeviceState(DeviceState::ERROR);
                isReconnecting_ = false;
            }
        }
    }
    
    LOG_DEBUG("Reconnection worker stopped");
}

void DeviceManager::printDeviceList(const std::string& prompt, std::shared_ptr<ob::DeviceList> deviceList) {
    if(!deviceList || deviceList->getCount() == 0) {
        return;
    }
    
    auto count = deviceList->getCount();
    LOG_INFO(count, " device(s) ", prompt, ":");
    
    for(uint32_t i = 0; i < count; i++) {
        auto uid          = deviceList->getUid(i);
        auto vid          = deviceList->getVid(i);
        auto pid          = deviceList->getPid(i);
        auto serialNumber = deviceList->getSerialNumber(i);
        auto connection   = deviceList->getConnectionType(i);
        
        LOG_INFO(" - uid: ", uid, 
                 ", vid: 0x", std::hex, std::setfill('0'), std::setw(4), vid,
                 ", pid: 0x", std::setw(4), pid,
                 ", serial: ", serialNumber,
                 ", connection: ", connection);
    }
} 