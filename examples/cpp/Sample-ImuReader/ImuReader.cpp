#include <iostream>
#include <mutex>
#include <libobsensor/ObSensor.hpp>
#include "utils.hpp"

#define ESC 27
std::mutex printerMutex;
int        main(int argc, char **argv) try {
    // Print the SDK version number, the SDK version number is divided into major version number, minor version number and revision number
    std::cout << "SDK version: " << ob::Version::getMajor() << "." << ob::Version::getMinor() << "." << ob::Version::getPatch() << std::endl;

    // Create a Context.
    ob::Context ctx;

    // Query the list of connected devices
    auto devList = ctx.queryDeviceList();

    if(devList->deviceCount() == 0) {
        std::cerr << "Device not found!" << std::endl;
        return -1;
    }

    // Create a device, 0 represents the index of the first device
    auto                        dev         = devList->getDevice(0);
    std::shared_ptr<ob::Sensor> gyroSensor  = nullptr;
    std::shared_ptr<ob::Sensor> accelSensor = nullptr;
    try {
        // Get Gyroscope Sensor
        gyroSensor = dev->getSensorList()->getSensor(OB_SENSOR_GYRO);
        if(gyroSensor) {
            // Get configuration list
            auto profiles = gyroSensor->getStreamProfileList();
            // Select the first profile to open stream
            auto profile = profiles->getProfile(OB_PROFILE_DEFAULT);
            gyroSensor->start(profile, [](std::shared_ptr<ob::Frame> frame) {
                std::unique_lock<std::mutex> lk(printerMutex);
                auto                         timeStamp = frame->timeStamp();
                auto                         index     = frame->index();
                auto                         gyroFrame = frame->as<ob::GyroFrame>();
                if(gyroFrame != nullptr && (index % 50) == 2) {  //( timeStamp % 500 ) < 2: Reduce printing frequency
                    auto value = gyroFrame->value();
                    std::cout << "Gyro Frame: \n\r{\n\r"
                              << "  tsp = " << timeStamp << "\n\r"
                              << "  temperature = " << gyroFrame->temperature() << "\n\r"
                              << "  gyro.x = " << value.x << " rad/s"
                              << "\n\r"
                              << "  gyro.y = " << value.y << " rad/s"
                              << "\n\r"
                              << "  gyro.z = " << value.z << " rad/s"
                              << "\n\r"
                              << "}\n\r" << std::endl;
                }
            });
        }
        else {
            std::cout << "get gyro Sensor failed ! " << std::endl;
        }
    }
    catch(ob::Error &e) {
        std::cerr << "current device is not support imu!" << std::endl;
        exit(EXIT_FAILURE);
    }

    // Get Acceleration Sensor
    accelSensor = dev->getSensorList()->getSensor(OB_SENSOR_ACCEL);
    if(accelSensor) {
        // Get configuration list
        auto profiles = accelSensor->getStreamProfileList();
        // Select the first profile to open stream
        auto profile = profiles->getProfile(OB_PROFILE_DEFAULT);
        accelSensor->start(profile, [](std::shared_ptr<ob::Frame> frame) {
            std::unique_lock<std::mutex> lk(printerMutex);
            auto                         timeStamp  = frame->timeStamp();
            auto                         index      = frame->index();
            auto                         accelFrame = frame->as<ob::AccelFrame>();
            if(accelFrame != nullptr && (index % 50) == 0) {
                auto value = accelFrame->value();
                std::cout << "Accel Frame: \n\r{\n\r"
                          << "  tsp = " << timeStamp << "\n\r"
                          << "  temperature = " << accelFrame->temperature() << "\n\r"
                          << "  accel.x = " << value.x << " m/s^2"
                          << "\n\r"
                          << "  accel.y = " << value.y << " m/s^2"
                          << "\n\r"
                          << "  accel.z = " << value.z << " m/s^2"
                          << "\n\r"
                          << "}\n\r" << std::endl;
            }
        });
    }
    else {
        std::cout << "get Accel Sensor failed ! " << std::endl;
    }

    std::cout << "Press ESC to exit! " << std::endl;

    while(true) {
        // Get the value of the pressed key, if it is the ESC key, exit the program
        int key = getch();
        if(key == ESC)
            break;
    }

    // turn off the flow
    if(gyroSensor) {
        gyroSensor->stop();
    }
    if(accelSensor) {
        accelSensor->stop();
    }

    return 0;
}
catch(ob::Error &e) {
    std::cerr << "function:" << e.getName() << "\nargs:" << e.getArgs() << "\nmessage:" << e.getMessage() << "\ntype:" << e.getExceptionType() << std::endl;
    exit(EXIT_FAILURE);
}