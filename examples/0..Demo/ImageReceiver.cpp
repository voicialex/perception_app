#include <chrono>
#include <fstream>
#include <iomanip>
#include <map>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>

#include "libobsensor/ObSensor.hpp"
#include "libobsensor/hpp/Error.hpp"
#include "utils.hpp"
#include "utils_opencv.hpp"
#include "MetadataHelper.hpp"
#include "DumpHelper.hpp"
#include "ConfigHelper.hpp"
#include "ImageReceiver.hpp"

ImageReceiver::ImageReceiver()
    : window_("MultiStream", ConfigHelper::getInstance().renderConfig.windowWidth, 
              ConfigHelper::getInstance().renderConfig.windowHeight, ob_smpl::ARRANGE_GRID) {
    window_.setKeyPressedCallback([this](int key) {
        if(key == 27) {
            std::cout << "ESC key pressed, exiting..." << std::endl;
            shouldExit_ = true;
        }
    });
    init();
}

bool ImageReceiver::init() {
    try {
        config_ = std::make_shared<ob::Config>();
        auto device = pipe_.getDevice();
        if(!device) {
            std::cerr << "Failed to get device" << std::endl;
            return false;
        }

        auto sensorList = device->getSensorList();
        if(!sensorList) {
            std::cerr << "Failed to get sensor list" << std::endl;
            return false;
        }

        for(uint32_t i = 0; i < sensorList->getCount(); ++i) {
            auto sensorType = sensorList->getSensorType(i);
            if(isVideoSensorTypeEnabled(sensorType)) {
                config_->enableStream(sensorType);
            }
        }

        if(ConfigHelper::getInstance().streamConfig.enableIMU) {
            imuPipeline_ = std::make_shared<ob::Pipeline>(device);
            imuConfig_ = std::make_shared<ob::Config>();
            imuConfig_->enableGyroStream();
            imuConfig_->enableAccelStream();
        }

        return true;
    }
    catch(ob::Error &e) {
        handleError(e);
        return false;
    }
}

void ImageReceiver::processFrameSet(std::shared_ptr<ob::FrameSet> frameset) {
    if(!frameset) return;

    std::unique_lock<std::mutex> lk(frameMutex_);
    for(uint32_t i = 0; i < frameset->frameCount(); ++i) {
        auto frame = frameset->getFrame(i);
        frameMap_[frame->type()] = frame;
        processFrame(frame);
    }
}

void ImageReceiver::processFrame(std::shared_ptr<ob::Frame> frame) {
    if(!frame) return;

    auto& config = ConfigHelper::getInstance();
    auto& metadataHelper = MetadataHelper::getInstance();
    auto& frameHelper = DumpHelper::getInstance();

    if(config.metadataConfig.enableMetadata && 
       frame->index() % config.metadataConfig.printInterval == 0) {
        metadataHelper.printMetadata(frame, config.metadataConfig.printInterval);
    }

    if(config.saveConfig.enableDump) {
        frameHelper.saveFrame(frame, config.saveConfig.dumpPath);
    }
}

void ImageReceiver::run() {
    try {
        pipe_.start(config_, [this](std::shared_ptr<ob::FrameSet> frameset) {
            processFrameSet(frameset);
        });

        if(ConfigHelper::getInstance().streamConfig.enableIMU && 
           imuPipeline_ && imuConfig_) {
            imuPipeline_->start(imuConfig_, [this](std::shared_ptr<ob::FrameSet> frameset) {
                std::lock_guard<std::mutex> lock(imuFrameMutex_);
                for(uint32_t i = 0; i < frameset->frameCount(); ++i) {
                    auto frame = frameset->getFrame(i);
                    imuFrameMap_[frame->type()] = frame;
                    processFrame(frame);
                }
            });
        }

        while(window_.run() && !shouldExit_) {
            std::vector<std::shared_ptr<const ob::Frame>> framesForRender;

            {
                std::lock_guard<std::mutex> lock(frameMutex_);
                for(auto &frame: frameMap_) {
                    framesForRender.push_back(std::const_pointer_cast<const ob::Frame>(frame.second));
                }
            }

            if(ConfigHelper::getInstance().streamConfig.enableIMU) {
                std::lock_guard<std::mutex> lock(imuFrameMutex_);
                for(auto &frame: imuFrameMap_) {
                    framesForRender.push_back(std::const_pointer_cast<const ob::Frame>(frame.second));
                }
            }

            if(ConfigHelper::getInstance().renderConfig.enableRendering) {
                window_.pushFramesToView(framesForRender);
            }
        }

        cleanup();
    }
    catch(ob::Error &e) {
        handleError(e);
        cleanup();
    }
    catch(const std::exception &e) {
        std::cerr << "Runtime error: " << e.what() << std::endl;
        cleanup();
    }
}

bool ImageReceiver::isVideoSensorTypeEnabled(OBSensorType sensorType) {
    auto& config = ConfigHelper::getInstance().streamConfig;
    switch(sensorType) {
        case OB_SENSOR_COLOR: return config.enableColor;
        case OB_SENSOR_DEPTH: return config.enableDepth;
        case OB_SENSOR_IR: return config.enableIR;
        case OB_SENSOR_IR_LEFT: return config.enableIRLeft;
        case OB_SENSOR_IR_RIGHT: return config.enableIRRight;
        default: return false;
    }
}

void ImageReceiver::handleError(ob::Error &e) {
    std::cerr << "Error:\n"
              << "function:" << e.getName() << "\n"
              << "args:" << e.getArgs() << "\n"
              << "message:" << e.getMessage() << "\n"
              << "type:" << e.getExceptionType() << std::endl;
}

void ImageReceiver::cleanup() {
    try {
        std::cout << "Stopping main pipeline..." << std::endl;
        pipe_.stop();

        if(imuPipeline_) {
            std::cout << "Stopping IMU pipeline..." << std::endl;
            imuPipeline_->stop();
        }

        {
            std::lock_guard<std::mutex> lock(frameMutex_);
            frameMap_.clear();
        }
        {
            std::lock_guard<std::mutex> lock(imuFrameMutex_);
            imuFrameMap_.clear();
        }

        std::cout << "Cleanup completed" << std::endl;
    }
    catch(const std::exception &e) {
        std::cerr << "Error in cleanup: " << e.what() << std::endl;
    }
}

ImageReceiver::~ImageReceiver() {
    cleanup();
}
