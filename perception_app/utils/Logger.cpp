#include "Logger.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <filesystem>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <cstring>

// 获取进程名的全局变量
extern char* __progname;

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

Logger::~Logger() {
    try {
        if (logger_) {
            logger_->flush();
            logger_.reset();
        }
        // 让spdlog自然清理，不主动调用shutdown()
    } catch (...) {
        // 析构函数中不抛异常
    }
}

bool Logger::initialize(Level level, bool enableConsole, const std::string& logFilePath) {
    if (initialized_) {
        return true; // 已经初始化过了
    }

    try {
        std::vector<spdlog::sink_ptr> sinks;

        // 添加控制台sink
        if (enableConsole) {
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e][%^%l%$][%t] %v");
            sinks.push_back(console_sink);
        }

        // 添加文件sink
        if (!logFilePath.empty()) {
            // 确保目录存在
            auto dir = std::filesystem::path(logFilePath).parent_path();
            if (!dir.empty()) {
                std::filesystem::create_directories(dir);
            }

            // 使用旋转文件sink，每个文件最大10MB，保留3个文件
            auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                logFilePath, 1024 * 1024 * 10, 3);
            file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e][%l][%t] %v");
            sinks.push_back(file_sink);
        }

        // 创建logger
        if (sinks.empty()) {
            // 如果没有sink，创建一个默认的控制台sink
            logger_ = spdlog::stdout_color_mt("perception_app");
        } else {
            logger_ = std::make_shared<spdlog::logger>("perception_app", begin(sinks), end(sinks));
        }

        // 设置级别
        setLevel(level);

        // 设置自动刷新
        logger_->flush_on(spdlog::level::warn);

        // 注册为默认logger
        spdlog::set_default_logger(logger_);

        initialized_ = true;
        
        // 初始化完成后记录一条日志
        info("Logger initialized successfully - level: ", static_cast<int>(level), 
             ", console: ", enableConsole, ", file: ", logFilePath.empty() ? "disabled" : logFilePath);
        
        return true;

    } catch (const std::exception& e) {
        // 如果初始化失败，创建一个简单的控制台logger
        try {
            logger_ = spdlog::stdout_color_mt("perception_app_fallback");
            logger_->error("Logger initialization failed: {}, using fallback console logger", e.what());
            initialized_ = true;
            return true;
        } catch (...) {
            return false;
        }
    }
}

bool Logger::initializeAdvanced(Level level, bool enableConsole, bool enableFileLogging, 
                               const std::string& logDirectory, const std::string& filePrefix, 
                               bool useTimestamp) {
    if (initialized_) {
        return true; // 已经初始化过了
    }

    try {
        std::vector<spdlog::sink_ptr> sinks;

        // 添加控制台sink
        if (enableConsole) {
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e][%^%l%$][%t] %v");
            sinks.push_back(console_sink);
        }

        // 添加文件sink（只有在启用文件日志时才处理）
        std::string actualLogFile = "";
        if (enableFileLogging) {
            // 处理日志目录
            std::string logDir = logDirectory.empty() ? "logs/" : logDirectory;
            
            // 确保目录路径以斜杠结尾
            if (!logDir.empty() && logDir.back() != '/' && logDir.back() != '\\') {
                logDir += '/';
            }
            
            // 创建目录
            try {
                std::filesystem::create_directories(logDir);
            } catch (const std::filesystem::filesystem_error& e) {
                // 如果创建失败，使用标准输出记录错误
                std::cerr << "Failed to create log directory: " << logDir << " - " << e.what() << std::endl;
                // 创建失败时不使用文件日志
                enableFileLogging = false;
            }
            
            // 只有在目录创建成功时才继续创建文件
            if (enableFileLogging) {
                // 生成文件名：优先使用进程名，如果获取失败则使用 filePrefix
                std::string baseName = getProcessName();
                if (baseName.empty() || baseName == "unknown") {
                    baseName = filePrefix.empty() ? "perception_app" : filePrefix;
                }
                
                std::string fileName = baseName;
                if (useTimestamp) {
                    auto now = std::chrono::system_clock::now();
                    auto time_t = std::chrono::system_clock::to_time_t(now);
                    std::ostringstream ss;
                    ss << baseName << "_" << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S");
                    fileName = ss.str();
                }
                fileName += ".log";
                
                // 完整的日志文件路径
                actualLogFile = logDir + fileName;
                
                // 使用旋转文件sink，每个文件最大10MB，保留3个文件
                auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                    actualLogFile, 1024 * 1024 * 10, 3);
                file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e][%l][%t] %v");
                sinks.push_back(file_sink);
            }
        }

        // 创建logger
        if (sinks.empty()) {
            // 如果没有sink，创建一个默认的控制台sink
            logger_ = spdlog::stdout_color_mt("perception_app");
        } else {
            logger_ = std::make_shared<spdlog::logger>("perception_app", begin(sinks), end(sinks));
        }

        // 设置级别
        setLevel(level);

        // 设置自动刷新
        logger_->flush_on(spdlog::level::warn);

        // 注册为默认logger
        spdlog::set_default_logger(logger_);

        initialized_ = true;
        
        // 初始化完成后记录一条日志
        info("Logger initialized successfully");
        info("  Level: ", static_cast<int>(level));
        info("  Console: ", enableConsole ? "enabled" : "disabled");
        info("  File logging: ", enableFileLogging ? "enabled" : "disabled");
        if (enableFileLogging && !actualLogFile.empty()) {
            info("  Log file: ", actualLogFile);
        }
        
        return true;

    } catch (const std::exception& e) {
        // 如果初始化失败，创建一个简单的控制台logger
        try {
            logger_ = spdlog::stdout_color_mt("perception_app_fallback");
            logger_->error("Advanced logger initialization failed: {}, using fallback console logger", e.what());
            initialized_ = true;
            return true;
        } catch (...) {
            return false;
        }
    }
}

void Logger::setLevel(Level level) {
    if (logger_) {
        logger_->set_level(toSpdlogLevel(level));
    }
}

void Logger::flush() {
    if (logger_) {
        logger_->flush();
    }
}

spdlog::level::level_enum Logger::toSpdlogLevel(Level level) const {
    switch (level) {
        case Level::DEBUG: return spdlog::level::debug;
        case Level::INFO:  return spdlog::level::info;
        case Level::WARN:  return spdlog::level::warn;
        case Level::ERROR: return spdlog::level::err;
        case Level::OFF:   return spdlog::level::off;
        default:           return spdlog::level::info;
    }
}

std::string Logger::ensureDirectoryExists(const std::string& path, bool addTrailingSlash) {
    try {
        if (path.empty()) {
            std::cerr << "Path is empty" << std::endl;
            return "";
        }
        
        // 规范化路径
        std::string normPath = path;
        
        // 确保路径末尾有斜杠（如果需要）
        if (addTrailingSlash && !normPath.empty() && normPath.back() != '/' && normPath.back() != '\\') {
            normPath += '/';
        }
        
        // 创建目录（如果不存在）
        if (!std::filesystem::exists(normPath)) {
            if (std::filesystem::create_directories(normPath)) {
                // 目录创建成功，但此时可能logger还未初始化，使用cout
                std::cout << "Directory created: " << normPath << std::endl;
            } else {
                std::cerr << "Failed to create directory: " << normPath << std::endl;
                return "";
            }
        }
        
        return normPath;
    }
    catch (const std::exception& e) {
        std::cerr << "Error processing directory: " << e.what() << std::endl;
        return "";
    }
}

std::string Logger::getProcessName() {
    try {
        if (__progname && strlen(__progname) > 0) {
            return std::string(__progname);
        }
        // 如果 __progname 为空或不可用，返回默认值
        return "perception_app";
    } catch (const std::exception& e) {
        std::cerr << "Error getting process name: " << e.what() << std::endl;
        return "perception_app";
    }
} 