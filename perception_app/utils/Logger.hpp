#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <mutex>
#include <thread>

/**
 * @brief Logger Manager - Singleton pattern
 * Provides a unified logging interface with support for different log levels
 */
class Logger {
public:
    enum class Level {
        DEBUG = 0,
        INFO = 1,
        WARN = 2,
        ERROR = 3
    };

    static Logger& getInstance() {
        static Logger instance;
        return instance;
    }

    void setLevel(const std::string& levelStr) {
        if(levelStr == "DEBUG") level_ = Level::DEBUG;
        else if(levelStr == "INFO") level_ = Level::INFO;
        else if(levelStr == "WARN") level_ = Level::WARN;
        else if(levelStr == "ERROR") level_ = Level::ERROR;
    }

    void setLogFile(const std::string& filename) {
        std::lock_guard<std::mutex> lock(mutex_);
        if(logFile_.is_open()) {
            logFile_.close();
        }
        if(!filename.empty()) {
            logFile_.open(filename, std::ios::app);
        }
    }

    template<typename... Args>
    void debug(Args&&... args) {
        log(Level::DEBUG, "DEBUG", std::forward<Args>(args)...);
    }

    template<typename... Args>
    void info(Args&&... args) {
        log(Level::INFO, "INFO", std::forward<Args>(args)...);
    }

    template<typename... Args>
    void warn(Args&&... args) {
        log(Level::WARN, "WARN", std::forward<Args>(args)...);
    }

    template<typename... Args>
    void error(Args&&... args) {
        log(Level::ERROR, "ERROR", std::forward<Args>(args)...);
    }

private:
    Logger() = default;
    ~Logger() {
        if(logFile_.is_open()) {
            logFile_.close();
        }
    }

    template<typename... Args>
    void log(Level msgLevel, const std::string& levelStr, Args&&... args) {
        if(msgLevel < level_) return;

        std::lock_guard<std::mutex> lock(mutex_);
        
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;

        auto threadId = std::this_thread::get_id();
        std::stringstream threadIdStr;
        threadIdStr << threadId;

        std::ostringstream oss;
        oss << "[" << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        oss << "." << std::setfill('0') << std::setw(3) << ms.count() << "]";
        oss << "[" << levelStr << "]";
        oss << "[TID:" << threadIdStr.str() << "] ";
        
        (oss << ... << args);
        oss << std::endl;

        std::string message = oss.str();
        
        if(logFile_.is_open()) {
            logFile_ << message;
            logFile_.flush();
        } else {
            // Direct use of std::cout is intentional here as this is the Logger implementation itself
            // Using the logging macros here would cause infinite recursion
            std::cout << message;
        }
    }

    Level level_ = Level::INFO;
    std::ofstream logFile_;
    std::mutex mutex_;
};

// Convenience macros
#define LOG_DEBUG(...) Logger::getInstance().debug(__VA_ARGS__)
#define LOG_INFO(...) Logger::getInstance().info(__VA_ARGS__)
#define LOG_WARN(...) Logger::getInstance().warn(__VA_ARGS__)
#define LOG_ERROR(...) Logger::getInstance().error(__VA_ARGS__) 