#include "FifoComm.hpp"
#include "Logger.hpp"
#include <chrono>
#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

// FifoCommImpl实现
FifoCommImpl::FifoCommImpl(const std::string& basePath, CommRole role)
    : basePath_(basePath), inPipePath_(basePath_ + "_in"), outPipePath_(basePath_ + "_out"),
      isServer_(false), role_(role), readFd_(-1), writeFd_(-1) {
    // 设置管道路径已在初始化列表中完成
}

FifoCommImpl::~FifoCommImpl() {
    cleanup();
}

bool FifoCommImpl::initialize() {
    return initialize(role_);
}

bool FifoCommImpl::initialize(CommRole role) {
    if (role == CommRole::SERVER) {
        if (initializeAsServer()) {
            isServer_ = true;
            LOG_INFO("FIFO通信初始化成功 (服务端模式)");
            return true;
        }
        LOG_ERROR("FIFO通信初始化失败，无法作为服务端初始化");
        return false;
    } else if (role == CommRole::CLIENT) {
        if (initializeAsClient()) {
            isServer_ = false;
            LOG_INFO("FIFO通信初始化成功 (客户端模式)");
            return true;
        }
        LOG_ERROR("FIFO通信初始化失败，无法作为客户端初始化");
        return false;
    } else {
        // AUTO 兼容原有逻辑
        if (initializeAsServer()) {
            isServer_ = true;
            LOG_INFO("FIFO通信初始化成功 (服务端模式)");
            return true;
        }
        if (initializeAsClient()) {
            isServer_ = false;
            LOG_INFO("FIFO通信初始化成功 (客户端模式)");
            return true;
        }
        LOG_ERROR("FIFO通信初始化失败，无法作为服务端或客户端初始化");
        return false;
    }
}

bool FifoCommImpl::initializeAsServer() {
    LOG_INFO("尝试以服务端模式初始化...");
    
    // 设置服务端标志
    isServer_ = true;
    
    // 创建管道
    if (!createPipes()) {
        LOG_ERROR("创建管道失败");
        return false;
    }
    
    // 打开管道
    if (!openPipesWithRetry()) {
        LOG_ERROR("打开管道失败，超过最大重试次数");
        return false;
    }
    
    return true;
}

bool FifoCommImpl::initializeAsClient() {
    LOG_INFO("尝试以客户端模式初始化...");
    
    // 设置客户端标志
    isServer_ = false;
    
    // 检查管道是否存在
    if (!checkPipesExist()) {
        LOG_ERROR("管道文件不存在，请确保服务端已启动");
        return false;
    }
    
    // 打开管道（添加重试机制）
    if (!openPipesWithRetry()) {
        LOG_ERROR("打开管道失败，超过最大重试次数");
        return false;
    }
    
    return true;
}

bool FifoCommImpl::checkPipesExist() {
    struct stat st;
    
    // 检查入站管道
    if (stat(inPipePath_.c_str(), &st) != 0) {
        LOG_WARN("入站管道不存在: ", inPipePath_);
        return false;
    } else if (!S_ISFIFO(st.st_mode)) {
        LOG_ERROR("入站管道路径存在但不是管道: ", inPipePath_);
        return false;
    }
    
    // 检查出站管道
    if (stat(outPipePath_.c_str(), &st) != 0) {
        LOG_WARN("出站管道不存在: ", outPipePath_);
        return false;
    } else if (!S_ISFIFO(st.st_mode)) {
        LOG_ERROR("出站管道路径存在但不是管道: ", outPipePath_);
        return false;
    }
    
    return true;
}

bool FifoCommImpl::createPipes() {
    // 确保目录存在
    std::string dirPath = basePath_.substr(0, basePath_.find_last_of('/'));
    if (!dirPath.empty()) {
        struct stat st;
        if (stat(dirPath.c_str(), &st) != 0) {
            // 目录不存在，尝试创建
            LOG_INFO("尝试创建目录: ", dirPath);
            if (mkdir(dirPath.c_str(), 0777) != 0) {
                LOG_ERROR("创建目录失败: ", strerror(errno));
                return false;
            }
        }
    }
    
    // 删除可能存在的旧管道
    unlink(inPipePath_.c_str());
    unlink(outPipePath_.c_str());
    
    // 创建入站管道
    if (mkfifo(inPipePath_.c_str(), 0666) == -1) {
        if (errno != EEXIST) {
            LOG_ERROR("创建入站管道失败: ", strerror(errno));
            return false;
        }
    }
    
    // 创建出站管道
    if (mkfifo(outPipePath_.c_str(), 0666) == -1) {
        if (errno != EEXIST) {
            LOG_ERROR("创建出站管道失败: ", strerror(errno));
            unlink(inPipePath_.c_str());
            return false;
        }
    }
    
    // 修改权限确保所有用户可读写
    chmod(inPipePath_.c_str(), 0666);
    chmod(outPipePath_.c_str(), 0666);
    
    LOG_INFO("创建管道成功");
    return true;
}

bool FifoCommImpl::openPipes() {
    if (isServer_) {
        // 服务端模式 - 修改管道打开顺序，先阻塞打开入站管道，再打开出站管道
        LOG_INFO("服务端: 阻塞打开入站管道");
        // 使用阻塞模式，等待客户端连接
        readFd_ = open(inPipePath_.c_str(), O_RDONLY);
        if (readFd_ == -1) {
            LOG_ERROR("服务端: 打开入站管道失败: ", strerror(errno));
            return false;
        }
        
        // 设置非阻塞模式
        int flags = fcntl(readFd_, F_GETFL, 0);
        fcntl(readFd_, F_SETFL, flags | O_NONBLOCK);
        
        LOG_INFO("服务端: 打开出站管道");
        writeFd_ = open(outPipePath_.c_str(), O_WRONLY | O_NONBLOCK);
        if (writeFd_ == -1) {
            LOG_ERROR("服务端: 打开出站管道失败: ", strerror(errno));
            close(readFd_);
            readFd_ = -1;
            return false;
        }
    } else {
        // 客户端模式 - 修改管道打开顺序，先阻塞打开出站管道，再打开入站管道
        LOG_INFO("客户端: 阻塞打开出站管道");
        // 使用阻塞模式，等待服务端连接
        writeFd_ = open(inPipePath_.c_str(), O_WRONLY); // 客户端的出站是服务端的入站
        if (writeFd_ == -1) {
            LOG_ERROR("客户端: 打开出站管道失败: ", strerror(errno));
            return false;
        }
        
        LOG_INFO("客户端: 打开入站管道");
        readFd_ = open(outPipePath_.c_str(), O_RDONLY | O_NONBLOCK); // 客户端的入站是服务端的出站
        if (readFd_ == -1) {
            LOG_ERROR("客户端: 打开入站管道失败: ", strerror(errno));
            close(writeFd_);
            writeFd_ = -1;
            return false;
        }
    }
    
    LOG_INFO("打开管道成功");
    // 通知成功建立连接
    isConnected_ = true;
    return true;
}

bool FifoCommImpl::openPipesWithRetry() {
    const int MAX_RETRIES = 5; // 减少重试次数
    const int RETRY_DELAY_MS = 1000; // 减少重试延迟
    
    for (int retry = 0; retry < MAX_RETRIES; retry++) {
        if (retry > 0) {
            LOG_INFO("尝试第 ", retry, " 次打开管道...");
            std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_DELAY_MS));
        }
        
        // 尝试打开管道
        if (openPipes()) {
            if (retry > 0) {
                LOG_INFO("在第 ", retry + 1, " 次尝试后成功打开管道");
            }
            return true;
        }
        
        // 关闭之前可能部分打开的文件描述符
        if (readFd_ != -1) {
            close(readFd_);
            readFd_ = -1;
        }
        
        if (writeFd_ != -1) {
            close(writeFd_);
            writeFd_ = -1;
        }
    }
    
    return false;
}

void FifoCommImpl::cleanup() {
    // 关闭文件描述符
    if (readFd_ != -1) {
        close(readFd_);
        readFd_ = -1;
    }
    
    if (writeFd_ != -1) {
        close(writeFd_);
        writeFd_ = -1;
    }
    
    // 如果是服务端，删除管道文件
    if (isServer_) {
        LOG_INFO("服务端：删除管道文件");
        unlink(inPipePath_.c_str());
        unlink(outPipePath_.c_str());
    }
    
    // 更新连接状态
    isConnected_ = false;
    
    LOG_INFO("清理管道完成");
}

bool FifoCommImpl::sendMessage(const std::string& message) {
    if (writeFd_ == -1) {
        LOG_ERROR("无法发送消息: 写管道未打开");
        return false;
    }
    
    std::string data = message + "\n";  // 添加分隔符方便接收端解析
    
    // 发送消息
    ssize_t bytesWritten = write(writeFd_, data.c_str(), data.size());
    if (bytesWritten != static_cast<ssize_t>(data.size())) {
        if (bytesWritten == -1) {
            LOG_ERROR("写入管道失败: ", strerror(errno));
            
            // 检查是否因为管道断开
            if (errno == EPIPE) {
                LOG_ERROR("管道已断开，接收端可能已关闭");
            }
        } else {
            LOG_ERROR("写入管道不完整: ", bytesWritten, "/", data.size());
        }
        return false;
    }
    
    return true;
}

bool FifoCommImpl::receiveMessage(std::string& message) {
    if (readFd_ == -1) {
        LOG_ERROR("无法接收消息: 读管道未打开");
        return false;
    }
    
    // 检查是否有足够的数据可以处理
    if (!partialData_.empty()) {
        // 查找一条完整消息
        size_t pos = partialData_.find('\n');
        if (pos != std::string::npos) {
            // 提取一条完整消息
            message = partialData_.substr(0, pos);
            partialData_ = partialData_.substr(pos + 1);
            
            // 如果缓冲区中还有大量数据未处理，记录日志
            if (partialData_.size() > 200) {
                LOG_WARN("FIFO接收缓冲区有大量数据待处理: ", partialData_.size(), " 字节");
            }
            
            return true;
        }
    }
    
    // 从管道读取数据
    char buffer[4096];
    ssize_t bytesRead = read(readFd_, buffer, sizeof(buffer) - 1);
    
    if (bytesRead > 0) {
        // 确保字符串结束
        buffer[bytesRead] = '\0';
        
        // 添加到部分数据中
        partialData_ += buffer;
        
        // 循环提取所有完整消息，直到找不到完整消息为止
        size_t pos = partialData_.find('\n');
        if (pos != std::string::npos) {
            // 提取一条完整消息
            message = partialData_.substr(0, pos);
            partialData_ = partialData_.substr(pos + 1);
            
            // 记录剩余待处理数据量
            if (!partialData_.empty()) {
                LOG_DEBUG("FIFO接收处理完一条消息后，缓冲区还有 ", partialData_.size(), " 字节数据待处理");
            }
            
            return true;
        }
    }
    else if (bytesRead == -1) {
        // 非阻塞模式下，无数据可读会返回-1和EAGAIN
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            LOG_ERROR("读取管道失败: ", strerror(errno));
        }
    }
    
    return false;
}

void FifoCommImpl::setReceiveTimeout(int milliseconds) {
    // FIFO实现不需要超时设置，使用非阻塞模式
    (void)milliseconds;
}

bool FifoCommImpl::isConnected() const {
    return isConnected_;
}

bool FifoCommImpl::isServer() const {
    return isServer_;
} 