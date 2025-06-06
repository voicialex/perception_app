#pragma once

#include <memory>
#include <functional>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <chrono>

#include "libobsensor/ObSensor.hpp"
#include "Logger.hpp"
#include "ConfigHelper.hpp"

/**
 * @brief 设备管理器
 * 负责设备的连接、断开、重连等管理功能
 */
class DeviceManager {
public:
    // 设备状态枚举
    enum class DeviceState {
        DISCONNECTED,
        CONNECTING,
        CONNECTED,
        RECONNECTING,
        ERROR
    };

    // 设备事件回调类型
    using DeviceEventCallback = std::function<void(DeviceState oldState, DeviceState newState, std::shared_ptr<ob::Device> device)>;
    using DeviceListCallback = std::function<void(std::shared_ptr<ob::DeviceList> removedList, std::shared_ptr<ob::DeviceList> addedList)>;

    DeviceManager();
    ~DeviceManager();

    /**
     * @brief 初始化设备管理器
     * @return true if successful
     */
    bool initialize();

    /**
     * @brief 启动设备管理
     */
    void start();

    /**
     * @brief 停止设备管理
     */
    void stop();

    /**
     * @brief 获取当前设备
     * @return current device or nullptr
     */
    std::shared_ptr<ob::Device> getCurrentDevice() const;

    /**
     * @brief 获取当前设备状态
     * @return current device state
     */
    DeviceState getDeviceState() const { return deviceState_; }

    /**
     * @brief 设置设备事件回调
     * @param callback device event callback function
     */
    void setDeviceEventCallback(DeviceEventCallback callback) {
        deviceEventCallback_ = callback;
    }

    /**
     * @brief 重启当前设备
     * @return true if successful
     */
    bool rebootCurrentDevice();

    /**
     * @brief 打印连接的设备列表
     */
    void printConnectedDevices();

    /**
     * @brief 等待设备连接
     * @param timeoutMs timeout in milliseconds, 0 means wait forever
     * @return true if device connected within timeout
     */
    bool waitForDevice(int timeoutMs = 0);

private:
    void setDeviceState(DeviceState newState);
    void setupHotPlugCallback();
    void onDeviceChanged(std::shared_ptr<ob::DeviceList> removedList, std::shared_ptr<ob::DeviceList> addedList);
    void handleDeviceDisconnection();
    void handleDeviceConnection();
    bool attemptConnection();
    void reconnectionWorker();
    void printDeviceList(const std::string& prompt, std::shared_ptr<ob::DeviceList> deviceList);

private:
    std::shared_ptr<ob::Context> context_;
    std::shared_ptr<ob::Device> currentDevice_;
    
    std::atomic<DeviceState> deviceState_{DeviceState::DISCONNECTED};
    std::atomic<bool> shouldStop_{false};
    std::atomic<bool> isReconnecting_{false};
    std::atomic<int> reconnectAttempts_{0};
    
    mutable std::mutex deviceMutex_;
    std::condition_variable deviceCondition_;
    std::thread reconnectionThread_;
    
    DeviceEventCallback deviceEventCallback_;
    
    std::chrono::steady_clock::time_point lastDisconnectTime_;
}; 