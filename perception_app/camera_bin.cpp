#include <iostream>
#include <memory>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>
#include <filesystem>

#include "ImageReceiver.hpp"
#include "ConfigHelper.hpp"
#include "Logger.hpp"

// Global variables for signal handling
std::atomic<bool> g_exitRequested{false};
std::unique_ptr<ImageReceiver> g_imageReceiver;

/**
 * @brief Signal handler function
 */
void signalHandler(int signal) {
    LOG_INFO("Signal caught: ", signal);
    g_exitRequested = true;
    
    if (g_imageReceiver) {
        g_imageReceiver->stop();
    }
    
    // Use a more graceful exit approach
    static std::atomic<bool> forceExitInProgress{false};
    
    if (!forceExitInProgress) {
        forceExitInProgress = true;
        std::thread([signal]() {
            // Wait a short time for normal program exit
            std::this_thread::sleep_for(std::chrono::milliseconds(800));
            if (g_exitRequested) {
                LOG_ERROR("Program did not exit in time, forcing exit");
                _exit(signal); // Use _exit instead of std::exit to avoid blocking in destructors
            }
        }).detach();
    }
}

/**
 * @brief Ensure directory exists, create if not
 * @param dirPath Directory path
 * @return Whether creation was successful or directory already exists
 */
bool ensureDirectoryExists(const std::string& dirPath) {
    try {
        if (dirPath.empty()) {
            LOG_ERROR("Directory path is empty!");
            return false;
        }
        
        // Check if directory exists, create if not
        if (!std::filesystem::exists(dirPath)) {
            LOG_INFO("Creating directory: ", dirPath);
            return std::filesystem::create_directories(dirPath);
        }
        
        // Check if it's a directory
        if (!std::filesystem::is_directory(dirPath)) {
            LOG_ERROR("Path exists but is not a directory: ", dirPath);
            return false;
        }
        
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Error creating directory: ", e.what());
        return false;
    }
}

int main() {
    try {
        // Set up signal handling
        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);

        // Set log level to DEBUG for more information
        Logger::getInstance().setLevel("DEBUG");
        
        LOG_INFO("=== Orbbec Camera Launcher ===");
        LOG_INFO("Starting up...");
        
        // Get configuration and modify key parameters
        auto& config = ConfigHelper::getInstance();
        
        // Modify configuration to ensure basic streams are open and debug output is enabled
        LOG_INFO("Configuring camera parameters...");
        config.streamConfig.enableColor = true;
        config.streamConfig.enableDepth = true;
        config.streamConfig.enableIR = true;      // 启用红外流
        config.renderConfig.enableRendering = true;
        config.renderConfig.showFPS = true;
        config.hotPlugConfig.enableHotPlug = true;
        config.hotPlugConfig.waitForDeviceOnStartup = true;
        config.hotPlugConfig.printDeviceEvents = true;
        config.debugConfig.enableDebugOutput = true;
        config.debugConfig.enablePerformanceStats = true;
        
        // Set data saving configuration
        config.saveConfig.enableDump = true;
        config.saveConfig.dumpPath = "./dumps/";  // Use dumps subdirectory in current directory
        config.saveConfig.saveColor = true;       // Save color images
        config.saveConfig.saveDepth = true;       // Save depth images
        config.saveConfig.saveIR = true;          // Save IR images
        config.saveConfig.imageFormat = "png";    // Use PNG format
        config.saveConfig.maxFramesToSave = 1000; // Maximum save 10,000 frames
        
        // Ensure save path exists
        if (!ensureDirectoryExists(config.saveConfig.dumpPath)) {
            LOG_ERROR("Cannot create data save directory, will use current directory");
            config.saveConfig.dumpPath = "./";
        }
        
        if (!config.validateAll()) {
            LOG_ERROR("Configuration validation failed!");
            return -1;
        }
        
        // Print current configuration
        config.printConfig();
        
        LOG_INFO("Waiting for device connection...");
        
        // Create and initialize image receiver
        g_imageReceiver = std::make_unique<ImageReceiver>();
        if (!g_imageReceiver->initialize()) {
            LOG_ERROR("Cannot initialize image receiver");
            return -1;
        }
        
        LOG_INFO("Camera initialized successfully, starting operation...");
        
        // Use new simplified interface to directly start stream processing
        LOG_INFO("Starting video stream processing...");
        if (!g_imageReceiver->startStreaming()) {
            LOG_INFO("Failed to start video stream, will rely on hot-plug mechanism, waiting 2 seconds...");
            std::this_thread::sleep_for(std::chrono::seconds(2));
        } else {
            LOG_INFO("Video stream started successfully!");
        }
        
        // Run main loop
        LOG_INFO("Starting main loop...");
        g_imageReceiver->run();
        
        // If normal exit, reset exit flag
        g_exitRequested = false;
        
        LOG_INFO("Program exited normally");
        return 0;
    }
    catch (const std::exception& e) {
        LOG_ERROR("Program error: ", e.what());
        return -1;
    }
    catch (...) {
        LOG_ERROR("Unknown error occurred");
        return -1;
    }
} 