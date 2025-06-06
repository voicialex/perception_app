#pragma once

#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <thread>
#include <functional>
#include <memory>
#include <map>
#include <chrono>
#include <vector>
#include "ICommunicationImpl.hpp"
#include "FifoComm.hpp"
#include "ThreadPool.hpp"

/**
 * @brief 通信代理类 - 单例模式
 */
class CommunicationProxy {
public:
    using CommRole = ICommunicationImpl::CommRole;
    
    /**
     * @brief 消息类型枚举
     */
    enum class MessageType {
        COMMAND,        // 命令消息
        STATUS_REPORT,  // 状态报告
        ERROR,          // 错误消息
        HEARTBEAT,      // 心跳消息
        METADATA,       // 元数据消息
        DATA            // 数据消息
    };
    
    /**
     * @brief 消息优先级枚举
     */
    enum class MessagePriority {
        HIGH,           // 高优先级（心跳、状态查询等）
        NORMAL,         // 普通优先级（一般命令）
        LOW             // 低优先级（非关键操作）
    };
    
    /**
     * @brief 连接状态枚举
     */
    enum class ConnectionState {
        DISCONNECTED,   // 未连接
        CONNECTING,     // 连接中
        CONNECTED       // 已连接
    };
    
    /**
     * @brief 消息结构体
     */
    struct Message {
        MessageType type;  // 消息类型
        std::string content;  // 消息内容
        MessagePriority priority; // 消息优先级
        
        Message() : type(MessageType::COMMAND), priority(MessagePriority::NORMAL) {}
        
        Message(MessageType t, const std::string& c, MessagePriority p = MessagePriority::NORMAL) 
            : type(t), content(c), priority(p) {}
        
        // 序列化为字符串
        std::string serialize() const {
            return std::to_string(static_cast<int>(type)) + ":" + content;
        }
        
        // 从字符串反序列化
        static Message deserialize(const std::string& data) {
            Message msg;
            size_t pos = data.find(':');
            if(pos != std::string::npos) {
                int type = std::stoi(data.substr(0, pos));
                msg.type = static_cast<MessageType>(type);
                msg.content = data.substr(pos + 1);
                
                // 设置默认优先级（心跳消息为高优先级）
                if (msg.type == MessageType::HEARTBEAT) {
                    msg.priority = MessagePriority::HIGH;
                }
            }
            return msg;
        }
    };
    
    /**
     * @brief 消息回调函数类型
     */
    using MessageCallback = std::function<void(const Message&)>;
    
    /**
     * @brief 连接状态变化回调函数类型
     */
    using ConnectionCallback = std::function<void(ConnectionState newState)>;
    
    /**
     * @brief 获取单例实例
     * @return 单例实例引用
     */
    static CommunicationProxy& getInstance();
    
    /**
     * @brief 初始化通信代理
     * @param basePath 通信管道基础路径名（可选）
     * @param role 通信角色
     * @return 是否成功
     */
    bool initialize(const std::string& basePath, CommRole role = CommRole::AUTO);
    
    /**
     * @brief 初始化通信代理
     * @return 是否成功
     */
    bool initialize();
    
    /**
     * @brief 启动通信代理
     */
    void start();
    
    /**
     * @brief 停止通信代理
     */
    void stop();
    
    /**
     * @brief 发送消息
     * @param type 消息类型
     * @param content 消息内容
     * @return 是否成功
     */
    bool sendMessage(MessageType type, const std::string& content);
    
    /**
     * @brief 注册消息回调
     * @param type 消息类型
     * @param callback 回调函数
     */
    void registerCallback(MessageType type, MessageCallback callback);
    
    /**
     * @brief 取消注册消息回调
     * @param type 消息类型
     */
    void unregisterCallback(MessageType type);
       
    /**
     * @brief 注册连接状态变化回调函数
     * @param callback 回调函数
     */
    void registerConnectionCallback(ConnectionCallback callback);
    
    /**
     * @brief 取消注册连接状态变化回调函数
     */
    void unregisterConnectionCallback();
    
    /**
     * @brief 获取当前连接状态
     * @return 连接状态
     */
    ConnectionState getConnectionState() const;
    
    /**
     * @brief 等待连接建立
     * @param timeoutMs 超时时间(毫秒)，0表示无限等待
     * @return 是否连接成功
     */
    bool waitForConnection(int timeoutMs = 0);

private:
    /**
     * @brief 构造函数(私有)
     */
    CommunicationProxy();
    
    /**
     * @brief 析构函数
     */
    ~CommunicationProxy();
    
    /**
     * @brief 消息接收线程函数
     */
    void messageReceivingThread();
    
    /**
     * @brief 处理接收到的消息
     * @param message 消息对象
     */
    void processReceivedMessage(const Message& message);
    
    /**
     * @brief 获取消息优先级
     * @param type 消息类型
     * @return 消息优先级
     */
    MessagePriority getMessagePriority(MessageType type);
    
    /**
     * @brief 设置连接状态
     * @param newState 新状态
     */
    void setConnectionState(ConnectionState newState);
    
    // 禁止拷贝构造和赋值
    CommunicationProxy(const CommunicationProxy&) = delete;
    CommunicationProxy& operator=(const CommunicationProxy&) = delete;
    
    std::atomic<bool> isInitialized_{false};  // 初始化标志
    std::atomic<bool> isRunning_{false};      // 运行标志
    
    // 通信实现
    std::unique_ptr<ICommunicationImpl> commImpl_;
    
    // 线程池
    std::unique_ptr<utils::ThreadPool> threadPool_;
    
    // 回调函数
    std::map<MessageType, MessageCallback> callbacks_; // 回调函数映射
    std::mutex callbackMutex_;                // 回调锁
    
    // 线程
    std::thread receivingThread_;             // 消息接收线程
    
    // 连接状态
    std::atomic<ConnectionState> connectionState_{ConnectionState::DISCONNECTED};
    ConnectionCallback connectionCallback_{nullptr};
    std::mutex connectionMutex_;
    std::condition_variable connectionCondition_;
};