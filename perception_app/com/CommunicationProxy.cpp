#include "CommunicationProxy.hpp"
#include "FifoComm.hpp"
#include "Logger.hpp"
#include <chrono>
#include <algorithm>
#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

// 线程池的默认线程数量
constexpr size_t DEFAULT_THREAD_POOL_SIZE = 3;

// CommunicationProxy 实现
CommunicationProxy& CommunicationProxy::getInstance() {
    static CommunicationProxy instance;
    return instance;
}

CommunicationProxy::CommunicationProxy() {
    // 构造函数
}

CommunicationProxy::~CommunicationProxy() {
    stop();
}

bool CommunicationProxy::initialize(const std::string& basePath, CommRole role) {
    if(isInitialized_) {
        LOG_WARN("通信代理已初始化");
        return true;
    }

    LOG_INFO("正在初始化通信代理...");
    
    // 设置初始连接状态为连接中
    setConnectionState(ConnectionState::CONNECTING);
    
    // 创建FIFO通信实现
    commImpl_ = std::make_unique<FifoCommImpl>(basePath, role);
    
    // 初始化通信实现
    if (!commImpl_->initialize()) {
        LOG_ERROR("Failed to initialize CommunicationProxy");
        setConnectionState(ConnectionState::DISCONNECTED);
        return false;
    }
    
    // 创建线程池
    threadPool_ = std::make_unique<utils::ThreadPool>(DEFAULT_THREAD_POOL_SIZE);
    
    isInitialized_ = true;
    LOG_INFO("通信代理初始化成功");
    return true;
}

bool CommunicationProxy::initialize() {
    return initialize("/tmp/orbbec_camera", CommRole::AUTO);
}

void CommunicationProxy::start() {
    if(!isInitialized_) {
        LOG_ERROR("无法启动通信代理: 未初始化");
        return;
    }

    if(isRunning_) {
        LOG_WARN("通信代理已在运行");
        return;
    }

    isRunning_ = true;
    LOG_INFO("正在启动通信代理...");

    // 启动消息接收线程
    receivingThread_ = std::thread(&CommunicationProxy::messageReceivingThread, this);
    
    // 根据角色启动不同的初始化流程
    if (commImpl_->isServer()) {
        LOG_INFO("服务端启动，等待客户端连接...");
    } else {
        // 客户端需要尝试连接到服务端
        LOG_INFO("客户端启动，尝试连接到服务端...");
    }
    
    LOG_INFO("通信代理已启动");
}

void CommunicationProxy::stop() {
    if(!isRunning_) {
        return;
    }

    LOG_INFO("正在停止通信代理...");
    isRunning_ = false;
    
    // 通知所有等待的线程
    {
        std::lock_guard<std::mutex> lock(connectionMutex_);
        connectionCondition_.notify_all();
    }
    
    // 等待线程结束
    if(receivingThread_.joinable()) {
        receivingThread_.join();
    }
    
    // 销毁线程池（析构函数会处理线程关闭）
    threadPool_.reset();
    
    // 清理通信资源
    if (commImpl_) {
        commImpl_->cleanup();
    }
    
    // 更新连接状态
    setConnectionState(ConnectionState::DISCONNECTED);
    
    LOG_INFO("通信代理已停止");
}

bool CommunicationProxy::sendMessage(MessageType type, const std::string& content) {
    if(!isRunning_) {
        LOG_ERROR("无法发送消息: 通信代理未运行");
        return false;
    }
    
    // 检查连接状态
    if (connectionState_ != ConnectionState::CONNECTED) {
        // 尝试建立连接
        if (type != MessageType::HEARTBEAT) {
            LOG_WARN("发送消息失败: 未连接，消息类型=", static_cast<int>(type), ", 内容=", content);
        }
        return false;
    }
    
    // 创建消息，设置优先级
    MessagePriority priority = getMessagePriority(type);
    Message message(type, content, priority);
    
    LOG_DEBUG("发送消息: type=", static_cast<int>(type), ", content=", content);
    
    // 序列化消息并通过通信实现发送
    bool result = commImpl_->sendMessage(message.serialize());
    
    // 如果发送失败，可能是连接断开
    if (!result) {
        setConnectionState(ConnectionState::DISCONNECTED);
    }
    
    return result;
}

void CommunicationProxy::registerCallback(MessageType type, MessageCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    callbacks_[type] = callback;
    LOG_DEBUG("注册了消息类型的回调: ", static_cast<int>(type));
}

void CommunicationProxy::unregisterCallback(MessageType type) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    callbacks_.erase(type);
    LOG_DEBUG("注销了消息类型的回调: ", static_cast<int>(type));
}

CommunicationProxy::MessagePriority CommunicationProxy::getMessagePriority(MessageType type) {
    // 为不同消息类型设置优先级
    switch (type) {
        case MessageType::HEARTBEAT:
            return MessagePriority::HIGH;  // 心跳消息最高优先级
            
        case MessageType::ERROR:
        case MessageType::STATUS_REPORT:
            return MessagePriority::HIGH;  // 错误和状态报告也是高优先级
            
        case MessageType::COMMAND:
            return MessagePriority::NORMAL;  // 命令消息普通优先级
            
        case MessageType::METADATA:
        case MessageType::DATA:
            return MessagePriority::LOW;  // 数据消息低优先级
            
        default:
            return MessagePriority::NORMAL;
    }
}

void CommunicationProxy::messageReceivingThread() {
    LOG_DEBUG("消息接收线程已启动");
    
    // 首次成功接收消息标志
    bool firstMessageReceived = false;
    
    // 消息处理计数
    int messagesProcessedInBatch = 0;
    const int MAX_MESSAGES_PER_BATCH = 10; // 单次循环最大处理消息数
    
    while(isRunning_) {
        try {
            // 重置批处理计数
            messagesProcessedInBatch = 0;
            
            // 使用轮询方式接收消息，循环处理所有可用消息
            while(isRunning_ && messagesProcessedInBatch < MAX_MESSAGES_PER_BATCH) {
                std::string messageData;
                if(commImpl_->receiveMessage(messageData)) {
                    // 如果是首次接收到消息，更新连接状态
                    if (!firstMessageReceived) {
                        firstMessageReceived = true;
                        setConnectionState(ConnectionState::CONNECTED);
                        LOG_INFO("成功接收第一条消息，连接已建立");
                    }
                    
                    // 反序列化消息
                    Message message = Message::deserialize(messageData);
                    
                    LOG_DEBUG("接收消息: type=", static_cast<int>(message.type), 
                             ", content=", message.content);
                    
                    // 对高优先级消息直接同步处理，其他消息提交给线程池异步处理
                    if (message.priority == MessagePriority::HIGH) {
                        // 高优先级消息同步处理（如心跳）
                        processReceivedMessage(message);
                    } else {
                        // 其他消息异步处理
                        Message msgCopy = message; // 创建消息的副本
                        threadPool_->submit([this, msgCopy]() {
                            processReceivedMessage(msgCopy);
                        });
                    }
                    
                    // 增加批处理计数
                    messagesProcessedInBatch++;
                } else {
                    // 没有更多消息可处理，退出内循环
                    break;
                }
            }
            
            // 如果单次循环处理了大量消息，记录日志
            if (messagesProcessedInBatch >= MAX_MESSAGES_PER_BATCH) {
                LOG_WARN("单次循环处理了大量消息: ", messagesProcessedInBatch, 
                        "，可能存在消息堆积");
            }
            
            // 检查连接状态
            if (commImpl_->isConnected()) {
                if (connectionState_ != ConnectionState::CONNECTED) {
                    setConnectionState(ConnectionState::CONNECTED);
                }
            } else {
                if (connectionState_ == ConnectionState::CONNECTED) {
                    setConnectionState(ConnectionState::DISCONNECTED);
                }
            }
            
            // 检查是否应该退出
            if(!isRunning_) {
                break;
            }
        }
        catch(const std::exception& e) {
            LOG_ERROR("消息接收线程异常: ", e.what());
            
            // 发生异常，可能是连接断开
            if (connectionState_ == ConnectionState::CONNECTED) {
                setConnectionState(ConnectionState::DISCONNECTED);
            }
        }
        
        // 短暂休眠，避免CPU占用过高
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    LOG_DEBUG("消息接收线程已停止");
}

void CommunicationProxy::processReceivedMessage(const Message& message) {
    // 调用对应类型的回调函数
    std::lock_guard<std::mutex> lock(callbackMutex_);
    auto it = callbacks_.find(message.type);
    if(it != callbacks_.end() && it->second) {
        try {
            it->second(message);
        }
        catch(const std::exception& e) {
            LOG_ERROR("消息回调异常: ", e.what());
        }
    }
}

// CommunicationProxy类中的连接状态相关函数
void CommunicationProxy::registerConnectionCallback(ConnectionCallback callback) {
    std::lock_guard<std::mutex> lock(connectionMutex_);
    connectionCallback_ = callback;
    
    // 如果已经连接，立即通知
    if (connectionState_ == ConnectionState::CONNECTED && callback) {
        callback(ConnectionState::CONNECTED);
    }
}

void CommunicationProxy::unregisterConnectionCallback() {
    std::lock_guard<std::mutex> lock(connectionMutex_);
    connectionCallback_ = nullptr;
}

CommunicationProxy::ConnectionState CommunicationProxy::getConnectionState() const {
    return connectionState_;
}

bool CommunicationProxy::waitForConnection(int timeoutMs) {
    std::unique_lock<std::mutex> lock(connectionMutex_);
    
    if (connectionState_ == ConnectionState::CONNECTED) {
        return true;
    }
    
    if (timeoutMs == 0) {
        // 无限等待
        connectionCondition_.wait(lock, [this]() {
            return connectionState_ == ConnectionState::CONNECTED || !isRunning_;
        });
    } else {
        // 有超时限制
        connectionCondition_.wait_for(lock, std::chrono::milliseconds(timeoutMs), [this]() {
            return connectionState_ == ConnectionState::CONNECTED || !isRunning_;
        });
    }
    
    return connectionState_ == ConnectionState::CONNECTED;
}

void CommunicationProxy::setConnectionState(ConnectionState newState) {
    ConnectionState oldState = connectionState_.exchange(newState);
    
    if (oldState != newState) {
        LOG_INFO("通信连接状态变化: ", static_cast<int>(newState));
        
        // 如果状态变为已连接，通知所有等待的线程
        if (newState == ConnectionState::CONNECTED) {
            std::lock_guard<std::mutex> lock(connectionMutex_);
            connectionCondition_.notify_all();
        }
        
        // 调用回调函数
        std::lock_guard<std::mutex> lock(connectionMutex_);
        if (connectionCallback_) {
            connectionCallback_(newState);
        }
    }
} 