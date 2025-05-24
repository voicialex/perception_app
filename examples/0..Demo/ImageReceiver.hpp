#pragma once

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

class ImageReceiver {
public:
    ImageReceiver();
    ~ImageReceiver();

    bool init();
    void run();

private:
    void processFrameSet(std::shared_ptr<ob::FrameSet> frameset);
    void processFrame(std::shared_ptr<ob::Frame> frame);
    void handleError(ob::Error &e);
    void cleanup();
    bool isVideoSensorTypeEnabled(OBSensorType sensorType);

private:
    // 主数据流 pipeline
    ob::Pipeline pipe_;
    std::shared_ptr<ob::Config> config_;

    // IMU 数据流 pipeline
    std::shared_ptr<ob::Pipeline> imuPipeline_;
    std::shared_ptr<ob::Config> imuConfig_;

    ob_smpl::CVWindow window_;

    std::mutex frameMutex_;
    std::mutex imuFrameMutex_;
    std::map<OBFrameType, std::shared_ptr<ob::Frame>> frameMap_;
    std::map<OBFrameType, std::shared_ptr<ob::Frame>> imuFrameMap_;

    bool shouldExit_ = false;
}; 