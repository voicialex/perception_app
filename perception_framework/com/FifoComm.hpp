#pragma once

#include <string>
#include <atomic>
#include <mutex>
#include <thread>
#include <functional>
#include "ICommunicationImpl.hpp"
#include "Logger.hpp"

/**
 * @brief FIFO通信实现类 - 使用命名管道实现进程间通信
 */
class FifoCommImpl : public ICommunicationImpl {
public:
    using CommRole = ICommunicationImpl::CommRole;

    /**
     * @brief 构造函数
     * @param basePath 管道基础路径
     * @param role 通信角色
     */
    explicit FifoCommImpl(const std::string& basePath, CommRole role = CommRole::AUTO);
    
    /**
     * @brief 析构函数
     */
    ~FifoCommImpl() override;
    
    /**
     * @brief 初始化通信
     * @param role 通信角色
     * @return 是否成功
     */
    bool initialize(CommRole role = CommRole::AUTO);
    
    /**
     * @brief 清理通信资源
     */
    void cleanup() override;
    
    /**
     * @brief 发送消息
     * @param message 消息内容
     * @return 是否成功
     */
    bool sendMessage(const std::string& message) override;
    
    /**
     * @brief 接收消息
     * @param message 接收到的消息
     * @return 是否成功
     */
    bool receiveMessage(std::string& message) override;
    
    /**
     * @brief 设置接收超时
     * @param milliseconds 超时时间(毫秒)
     */
    void setReceiveTimeout(int milliseconds) override;
    
    /**
     * @brief 获取连接状态
     * @return 是否已连接
     */
    bool isConnected() const override;
    
    /**
     * @brief 获取服务端标志
     * @return 是否为服务端
     */
    bool isServer() const override;
    
    // 实现无参数 initialize 接口
    bool initialize() override;
    
private:
    /**
     * @brief 尝试作为服务端初始化
     * @return 是否成功
     */
    bool initializeAsServer();
    
    /**
     * @brief 尝试作为客户端初始化
     * @return 是否成功
     */
    bool initializeAsClient();
    
    /**
     * @brief 创建管道
     * @return 是否成功
     */
    bool createPipes();
    
    /**
     * @brief 打开管道
     * @return 是否成功
     */
    bool openPipes();
    
    /**
     * @brief 检查管道是否存在
     * @return 是否存在
     */
    bool checkPipesExist();
    
    /**
     * @brief 带重试机制的打开管道
     * @return 是否成功
     */
    bool openPipesWithRetry();
    
private:
    std::string basePath_;              // 基础路径
    std::string inPipePath_;            // 输入管道路径
    std::string outPipePath_;           // 输出管道路径
    bool isServer_{false};              // 是否服务端
    CommRole role_{CommRole::AUTO};     // 通信角色
    int readFd_{-1};                    // 读文件描述符
    int writeFd_{-1};                   // 写文件描述符
    std::string partialData_;           // 部分数据(未完成的消息)
    std::atomic<bool> isConnected_{false}; // 连接状态
}; 