#include "MetadataHelper.hpp"
#include "DumpHelper.hpp"
#include "Logger.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>

MetadataHelper& MetadataHelper::getInstance() {
    static MetadataHelper instance;
    return instance;
}

void MetadataHelper::printMetadata(std::shared_ptr<ob::Frame> frame, int interval) {
    if(!frame) return;

    LOG_INFO("Frame ", frame->getIndex(), " metadata (every ", interval, " frames):");
    LOG_INFO("----------------------------------------");
    LOG_INFO("Frame Type: ", frameTypeName(frame->getType()));
    
    for(uint32_t i = 0; i < static_cast<uint32_t>(OB_FRAME_METADATA_TYPE_COUNT); i++) {
        auto type = static_cast<OBFrameMetadataType>(i);
        if(frame->hasMetadata(type)) {
            // Creating a formatted string for consistent spacing
            std::stringstream ss;
            ss << std::left << std::setw(50) << metadataTypeToString(type);
            LOG_INFO("metadata type: ", ss.str(), " metadata value: ", frame->getMetadataValue(type));
        }
    }
    LOG_INFO("----------------------------------------");
}

std::string MetadataHelper::extractMetadataToString(std::shared_ptr<ob::Frame> frame) {
    if (!frame) return "";
    
    std::stringstream ss;
    
    try {
        // 基本帧信息
        ss << extractFrameInfo(frame);
    } catch (const std::exception& e) {
        LOG_ERROR("Error in extractFrameInfo: ", e.what());
        return "Error in extractFrameInfo: " + std::string(e.what());
    }
    
    try {
        // 时间戳信息
        auto ts = DumpHelper::TimeStamp::extract(frame);
        ss << "\nTimestamp Information\n";
        ss << "====================\n";
        ss << "Device Timestamp: " << ts.deviceUs << " us (" << ts.deviceStr << ")\n";
        ss << "System Timestamp: " << ts.systemUs << " us (" << ts.systemStr << ")\n";
        ss << "Global Timestamp: " << ts.globalUs << " us\n";
    } catch (const std::exception& e) {
        LOG_ERROR("Error in TimeStamp::extract: ", e.what());
        return "Error in TimeStamp::extract: " + std::string(e.what());
    }
    
    try {
        // 根据帧类型提取特定信息
        switch (frame->getType()) {
            case OB_FRAME_COLOR:
                LOG_DEBUG("Processing COLOR frame");
                try {
                    auto videoFrame = frame->as<ob::VideoFrame>();
                    if (videoFrame) {
                        LOG_DEBUG("Successfully converted to VideoFrame, calling extractVideoFrameInfo");
                        ss << extractVideoFrameInfo(videoFrame);
                    } else {
                        LOG_WARN("Failed to convert COLOR frame to VideoFrame");
                        ss << "\nColor Frame: Failed to convert to VideoFrame\n";
                    }
                } catch (const std::exception& e) {
                    LOG_ERROR("Error processing COLOR frame: ", e.what());
                    ss << "\nColor Frame: ERROR - " << e.what() << "\n";
                }
                break;
                
            case OB_FRAME_IR:
            case OB_FRAME_IR_LEFT:
            case OB_FRAME_IR_RIGHT:
                LOG_DEBUG("Processing IR frame, type: ", static_cast<int>(frame->getType()));
                try {
                    auto videoFrame = frame->as<ob::VideoFrame>();
                    if (videoFrame) {
                        ss << extractVideoFrameInfo(videoFrame);
                    }
                } catch (const std::exception& e) {
                    LOG_ERROR("Error processing IR frame: ", e.what());
                    ss << "\nIR Frame: ERROR - " << e.what() << "\n";
                }
                break;
                
            case OB_FRAME_DEPTH: {
                LOG_DEBUG("Processing DEPTH frame");
                try {
                    auto videoFrame = frame->as<ob::VideoFrame>();
                    auto depthFrame = frame->as<ob::DepthFrame>();
                    if (videoFrame) {
                        ss << extractVideoFrameInfo(videoFrame);
                    }
                    if (depthFrame) {
                        ss << extractDepthFrameInfo(depthFrame);
                    }
                } catch (const std::exception& e) {
                    LOG_ERROR("Error processing DEPTH frame: ", e.what());
                    ss << "\nDepth Frame: ERROR - " << e.what() << "\n";
                }
                break;
            }
            case OB_FRAME_POINTS: {
                LOG_DEBUG("Processing POINTS frame");
                try {
                    auto pointsFrame = frame->as<ob::PointsFrame>();
                    if (pointsFrame) {
                        ss << extractPointsFrameInfo(pointsFrame);
                    }
                } catch (const std::exception& e) {
                    LOG_ERROR("Error processing POINTS frame: ", e.what());
                    ss << "\nPoints Frame: ERROR - " << e.what() << "\n";
                }
                break;
            }
            case OB_FRAME_ACCEL:
            case OB_FRAME_GYRO:
                LOG_DEBUG("Processing IMU frame, type: ", static_cast<int>(frame->getType()));
                try {
                    ss << extractIMUFrameInfo(frame);
                } catch (const std::exception& e) {
                    LOG_ERROR("Error processing IMU frame: ", e.what());
                    ss << "\nIMU Frame: ERROR - " << e.what() << "\n";
                }
                break;
            default:
                LOG_DEBUG("Unknown or unsupported frame type: ", static_cast<int>(frame->getType()));
                break;
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Error in extracting frame type specific info: ", e.what());
        return "Error in extracting frame type specific info: " + std::string(e.what());
    }
    
    try {
        // 设备元数据
        ss << extractDeviceMetadata(frame);
    } catch (const std::exception& e) {
        LOG_ERROR("Error in extractDeviceMetadata: ", e.what());
        return "Error in extractDeviceMetadata: " + std::string(e.what());
    }
    
    return ss.str();
}

std::string MetadataHelper::extractFrameInfo(std::shared_ptr<ob::Frame> frame) {
    if (!frame) return "";
    
    std::stringstream ss;
    ss << "Frame Information\n";
    ss << "================\n";
    
    try {
        ss << "Frame Type: " << frameTypeName(frame->getType()) << "\n";
    } catch (const std::exception& e) {
        LOG_ERROR("Error getting frame type: ", e.what());
        return "Error getting frame type: " + std::string(e.what());
    }
    
    try {
        OBFormat format = frame->getFormat();
        LOG_DEBUG("Frame format: ", static_cast<int>(format));
        ss << "Frame Format: " << formatName(format) << "\n";
    } catch (const std::exception& e) {
        LOG_ERROR("Error getting frame format: ", e.what());
        return "Error getting frame format: " + std::string(e.what());
    }
    
    try {
        ss << "Frame Index: " << frame->getIndex() << "\n";
    } catch (const std::exception& e) {
        LOG_ERROR("Error getting frame index: ", e.what());
        return "Error getting frame index: " + std::string(e.what());
    }
    
    try {
        ss << "Data Size: " << frame->getDataSize() << " bytes\n";
    } catch (const std::exception& e) {
        LOG_ERROR("Error getting frame data size: ", e.what());
        return "Error getting frame data size: " + std::string(e.what());
    }
    
    return ss.str();
}

std::string MetadataHelper::extractDeviceMetadata(std::shared_ptr<ob::Frame> frame) {
    if (!frame) return "";
    
    std::stringstream ss;
    ss << "\nDevice Metadata\n";
    ss << "===============\n";
    
    bool hasAnyMetadata = false;
    for (uint32_t i = 0; i < static_cast<uint32_t>(OB_FRAME_METADATA_TYPE_COUNT); i++) {
        auto type = static_cast<OBFrameMetadataType>(i);
        if (frame->hasMetadata(type)) {
            hasAnyMetadata = true;
            ss << metadataTypeToString(type) << ": " << frame->getMetadataValue(type) << "\n";
        }
    }
    
    if (!hasAnyMetadata) {
        ss << "No device metadata available\n";
    }
    
    return ss.str();
}

std::string MetadataHelper::extractVideoFrameInfo(std::shared_ptr<ob::VideoFrame> frame) {
    if (!frame) return "";
    
    std::stringstream ss;
    ss << "\nVideo Frame Information\n";
    ss << "======================\n";
    
    try {
        uint32_t width = frame->getWidth();
        ss << "Width: " << width << " pixels\n";
    } catch (const std::exception& e) {
        LOG_ERROR("Error getting frame width: ", e.what());
        ss << "Width: ERROR - " << e.what() << "\n";
    }
    
    try {
        uint32_t height = frame->getHeight();
        ss << "Height: " << height << " pixels\n";
    } catch (const std::exception& e) {
        LOG_ERROR("Error getting frame height: ", e.what());
        ss << "Height: ERROR - " << e.what() << "\n";
    }
    
    try {
        OBPixelType pixelType = frame->getPixelType();
        ss << "Pixel Type: " << static_cast<int>(pixelType) << "\n";
    } catch (const std::exception& e) {
        LOG_WARN("getPixelType() not supported for this frame format, skipping");
        ss << "Pixel Type: Not supported for this format\n";
    }
    
    try {
        uint8_t bitSize = frame->getPixelAvailableBitSize();
        ss << "Pixel Available Bit Size: " << static_cast<int>(bitSize) << "\n";
    } catch (const std::exception& e) {
        LOG_WARN("getPixelAvailableBitSize() not supported for this frame format, skipping");
        ss << "Pixel Available Bit Size: Not supported for this format\n";
    }
    
    return ss.str();
}

std::string MetadataHelper::extractDepthFrameInfo(std::shared_ptr<ob::DepthFrame> frame) {
    if (!frame) return "";
    
    std::stringstream ss;
    ss << "\nDepth Frame Information\n";
    ss << "======================\n";
    
    try {
        float scale = frame->getValueScale();
        ss << "Value Scale: " << scale << " mm\n";
    } catch (const std::exception& e) {
        LOG_WARN("getValueScale() not supported for this depth frame format, skipping");
        ss << "Value Scale: Not supported for this format\n";
    }
    
    return ss.str();
}

std::string MetadataHelper::extractPointsFrameInfo(std::shared_ptr<ob::PointsFrame> frame) {
    if (!frame) return "";
    
    std::stringstream ss;
    ss << "\nPoints Frame Information\n";
    ss << "=======================\n";
    ss << "Coordinate Value Scale: " << frame->getCoordinateValueScale() << "\n";
    ss << "Width: " << frame->getWidth() << "\n";
    ss << "Height: " << frame->getHeight() << "\n";
    
    return ss.str();
}

std::string MetadataHelper::extractIMUFrameInfo(std::shared_ptr<ob::Frame> frame) {
    if (!frame) return "";
    
    std::stringstream ss;
    
    if (frame->getType() == OB_FRAME_ACCEL) {
        auto accelFrame = frame->as<ob::AccelFrame>();
        if (accelFrame) {
            auto value = accelFrame->getValue();
            ss << "\nAccelerometer Information\n";
            ss << "========================\n";
            ss << "Acceleration (m/s²): X=" << value.x << ", Y=" << value.y << ", Z=" << value.z << "\n";
            ss << "Temperature: " << accelFrame->getTemperature() << " °C\n";
        }
    } else if (frame->getType() == OB_FRAME_GYRO) {
        auto gyroFrame = frame->as<ob::GyroFrame>();
        if (gyroFrame) {
            auto value = gyroFrame->getValue();
            ss << "\nGyroscope Information\n";
            ss << "====================\n";
            ss << "Angular Velocity (rad/s): X=" << value.x << ", Y=" << value.y << ", Z=" << value.z << "\n";
            ss << "Temperature: " << gyroFrame->getTemperature() << " °C\n";
        }
    }
    
    return ss.str();
}

std::string MetadataHelper::metadataTypeToString(OBFrameMetadataType type) {
    switch(type) {
        case OB_FRAME_METADATA_TYPE_TIMESTAMP: return "Timestamp";
        case OB_FRAME_METADATA_TYPE_SENSOR_TIMESTAMP: return "Sensor Timestamp";
        case OB_FRAME_METADATA_TYPE_FRAME_NUMBER: return "Frame Number";
        case OB_FRAME_METADATA_TYPE_AUTO_EXPOSURE: return "Auto Exposure";
        case OB_FRAME_METADATA_TYPE_EXPOSURE: return "Exposure";
        case OB_FRAME_METADATA_TYPE_GAIN: return "Gain";
        case OB_FRAME_METADATA_TYPE_AUTO_WHITE_BALANCE: return "Auto White Balance";
        case OB_FRAME_METADATA_TYPE_WHITE_BALANCE: return "White Balance";
        case OB_FRAME_METADATA_TYPE_BRIGHTNESS: return "Brightness";
        case OB_FRAME_METADATA_TYPE_CONTRAST: return "Contrast";
        case OB_FRAME_METADATA_TYPE_SATURATION: return "Saturation";
        case OB_FRAME_METADATA_TYPE_SHARPNESS: return "Sharpness";
        case OB_FRAME_METADATA_TYPE_BACKLIGHT_COMPENSATION: return "Backlight Compensation";
        case OB_FRAME_METADATA_TYPE_HUE: return "Hue";
        case OB_FRAME_METADATA_TYPE_GAMMA: return "Gamma";
        case OB_FRAME_METADATA_TYPE_POWER_LINE_FREQUENCY: return "Power Line Frequency";
        case OB_FRAME_METADATA_TYPE_LOW_LIGHT_COMPENSATION: return "Low Light Compensation";
        case OB_FRAME_METADATA_TYPE_MANUAL_WHITE_BALANCE: return "Manual White Balance";
        case OB_FRAME_METADATA_TYPE_ACTUAL_FRAME_RATE: return "Actual Frame Rate";
        case OB_FRAME_METADATA_TYPE_FRAME_RATE: return "Frame Rate";
        case OB_FRAME_METADATA_TYPE_AE_ROI_LEFT: return "AE ROI Left";
        case OB_FRAME_METADATA_TYPE_AE_ROI_TOP: return "AE ROI Top";
        case OB_FRAME_METADATA_TYPE_AE_ROI_RIGHT: return "AE ROI Right";
        case OB_FRAME_METADATA_TYPE_AE_ROI_BOTTOM: return "AE ROI Bottom";
        case OB_FRAME_METADATA_TYPE_EXPOSURE_PRIORITY: return "Exposure Priority";
        case OB_FRAME_METADATA_TYPE_HDR_SEQUENCE_NAME: return "HDR Sequence Name";
        case OB_FRAME_METADATA_TYPE_HDR_SEQUENCE_SIZE: return "HDR Sequence Size";
        case OB_FRAME_METADATA_TYPE_HDR_SEQUENCE_INDEX: return "HDR Sequence Index";
        case OB_FRAME_METADATA_TYPE_LASER_POWER: return "Laser Power";
        case OB_FRAME_METADATA_TYPE_LASER_POWER_LEVEL: return "Laser Power Level";
        case OB_FRAME_METADATA_TYPE_LASER_STATUS: return "Laser Status";
        case OB_FRAME_METADATA_TYPE_GPIO_INPUT_DATA: return "GPIO Input Data";
        case OB_FRAME_METADATA_TYPE_DISPARITY_SEARCH_OFFSET: return "Disparity Search Offset";
        case OB_FRAME_METADATA_TYPE_DISPARITY_SEARCH_RANGE: return "Disparity Search Range";
        default: return "Unknown Metadata Type";
    }
}