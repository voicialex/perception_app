#include "MetadataHelper.hpp"
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

    LOG_INFO("Frame ", frame->index(), " metadata (every ", interval, " frames):");
    LOG_INFO("----------------------------------------");
    LOG_INFO("Frame Type: ", ob::TypeHelper::convertOBFrameTypeToString(frame->type()));
    
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