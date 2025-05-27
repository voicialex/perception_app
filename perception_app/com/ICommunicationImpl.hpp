#pragma once

#include <string>

/**
 * @brief 通信接口 - 定义通信方法
 */
class ICommunicationImpl {
public:
    /**
     * @brief 通信角色枚举
     */
    enum class CommRole {
        AUTO,
        SERVER,
        CLIENT
    };

    virtual ~ICommunicationImpl() = default;
    
    // 初始化通信
    virtual bool initialize() = 0;
    
    // 清理通信资源
    virtual void cleanup() = 0;
    
    // 发送消息
    virtual bool sendMessage(const std::string& message) = 0;
    
    // 接收消息
    virtual bool receiveMessage(std::string& message) = 0;
    
    // 设置接收超时
    virtual void setReceiveTimeout(int milliseconds) = 0;
    
    // 获取连接状态
    virtual bool isConnected() const = 0;
    
    // 获取服务端标志
    virtual bool isServer() const = 0;
}; 