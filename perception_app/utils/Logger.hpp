#pragma once

#include <memory>
#include <string>
#include <sstream>
#include <spdlog/spdlog.h>

/**
 * @brief 简化的日志管理器 - 基于spdlog的单例模式
 * 
 * 设计原则：
 * - 简单易用：支持多种日志接口风格
 * - 向后兼容：支持原有的参数串联风格
 * - 线程安全：使用spdlog的线程安全特性
 * - 异常安全：析构函数不抛异常
 * - 信号安全：提供可在信号处理函数中使用的接口
 */
class Logger {
public:
    /**
     * @brief 日志级别枚举
     */
    enum class Level {
        DEBUG = 0,
        INFO = 1,
        WARN = 2,
        ERROR = 3,
        OFF = 4
    };

    /**
     * @brief 获取单例实例
     */
    static Logger& getInstance();

    /**
     * @brief 初始化日志系统
     * @param level 日志级别
     * @param enableConsole 是否启用控制台输出
     * @param logFilePath 日志文件路径（空字符串表示不输出到文件）
     */
    bool initialize(Level level = Level::INFO, 
                   bool enableConsole = true, 
                   const std::string& logFilePath = "");

    /**
     * @brief 高级初始化日志系统 - 支持目录路径和自动文件名生成
     * @param level 日志级别
     * @param enableConsole 是否启用控制台输出
     * @param enableFileLogging 是否启用文件日志
     * @param logDirectory 日志目录路径（空字符串表示当前目录下的logs/）
     * @param filePrefix 日志文件前缀（默认为"perception_app"）
     * @param useTimestamp 是否在文件名中添加时间戳
     */
    bool initializeAdvanced(Level level = Level::INFO,
                          bool enableConsole = true,
                          bool enableFileLogging = true,
                          const std::string& logDirectory = "",
                          const std::string& filePrefix = "perception_app",
                          bool useTimestamp = true);

    /**
     * @brief 检查是否已初始化
     */
    bool isInitialized() const { return initialized_; }

    /**
     * @brief 兼容老版本的多参数日志接口 - 使用stringstream连接
     */
    template<typename... Args>
    void debug(Args&&... args) {
        if (logger_) {
            std::string message = buildMessage(std::forward<Args>(args)...);
            logger_->debug(message);
        }
    }

    template<typename... Args>
    void info(Args&&... args) {
        if (logger_) {
            std::string message = buildMessage(std::forward<Args>(args)...);
            logger_->info(message);
        }
    }

    template<typename... Args>
    void warn(Args&&... args) {
        if (logger_) {
            std::string message = buildMessage(std::forward<Args>(args)...);
            logger_->warn(message);
        }
    }

    template<typename... Args>
    void error(Args&&... args) {
        if (logger_) {
            std::string message = buildMessage(std::forward<Args>(args)...);
            logger_->error(message);
        }
    }

    /**
     * @brief 设置日志级别
     */
    void setLevel(Level level);

    /**
     * @brief 刷新日志缓冲区
     */
    void flush();

    /**
     * @brief 静态工具方法：确保目录存在
     * @param path 目录路径
     * @param addTrailingSlash 是否确保路径末尾有斜杠
     * @return 规范化后的路径，如果创建失败则返回空字符串
     */
    static std::string ensureDirectoryExists(const std::string& path, bool addTrailingSlash = true);

    /**
     * @brief 获取进程名字（使用 __progname__ 全局变量）
     * @return 进程名字字符串
     */
    static std::string getProcessName();

private:
    Logger() = default;
    ~Logger();
    
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    std::shared_ptr<spdlog::logger> logger_;
    bool initialized_ = false;
    
    // 将级别转换为spdlog级别
    spdlog::level::level_enum toSpdlogLevel(Level level) const;
    
    // 内部消息构建方法 - 支持任意类型的参数串联
    template<typename... Args>
    std::string buildMessage(Args&&... args) {
        std::ostringstream oss;
        (oss << ... << args);  // C++17 fold expression
        return oss.str();
    }
};

// 便利宏定义 - 保持向后兼容
#define LOG_DEBUG(...) Logger::getInstance().debug(__VA_ARGS__)
#define LOG_INFO(...) Logger::getInstance().info(__VA_ARGS__)
#define LOG_WARN(...) Logger::getInstance().warn(__VA_ARGS__)
#define LOG_ERROR(...) Logger::getInstance().error(__VA_ARGS__)

// 信号安全的日志宏（只接受字符串字面量）
#define LOG_SIGNAL_DEBUG(msg) Logger::getInstance().debug(std::string(msg))
#define LOG_SIGNAL_INFO(msg) Logger::getInstance().info(std::string(msg))
#define LOG_SIGNAL_WARN(msg) Logger::getInstance().warn(std::string(msg))
#define LOG_SIGNAL_ERROR(msg) Logger::getInstance().error(std::string(msg)) 