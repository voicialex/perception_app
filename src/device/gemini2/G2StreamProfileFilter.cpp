// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "G2StreamProfileFilter.hpp"

#include "utils/Utils.hpp"
#include "stream/StreamProfile.hpp"
#include "exception/ObException.hpp"
#include "property/InternalProperty.hpp"

namespace libobsensor {
G2StreamProfileFilter::G2StreamProfileFilter(IDevice *owner) : DeviceComponentBase(owner) {
    fetchEffectiveStreamProfiles();
}

static bool isMatch(OBSensorType sensorType, std::shared_ptr<const VideoStreamProfile> videoProfile, OBEffectiveStreamProfile effProfile) {
    bool isSensorTypeEqual = sensorType == effProfile.sensorType;

    // Compatibility processing, normal mode has IR, but no LeftIR, RightIR; calibration mode has no IR, but has LeftIR, RightIR
    // IR and LeftIR both use the IR channel, so they are the same. RightIR uses the Depth channel, so RightIR must match accurately
    if(!isSensorTypeEqual && sensorType != OB_SENSOR_IR_RIGHT && effProfile.sensorType != OB_SENSOR_IR_RIGHT) {
        isSensorTypeEqual = isIRSensor(sensorType) && isIRSensor(effProfile.sensorType);
    }

    return isSensorTypeEqual && (videoProfile->getFormat() == effProfile.format) && (videoProfile->getFps() <= effProfile.maxFps)
           && (videoProfile->getWidth() == effProfile.width) && (videoProfile->getHeight() == effProfile.height);
}

StreamProfileList G2StreamProfileFilter::filter(const StreamProfileList &profiles) const {
    StreamProfileList filteredProfiles;

    for(const auto &profile: profiles) {
        if(!profile->is<VideoStreamProfile>()) {
            filteredProfiles.push_back(profile);
            continue;
        }
        auto videoProfile = profile->as<VideoStreamProfile>();
        auto streamType   = profile->getType();
        auto sensorType   = utils::mapStreamTypeToSensorType(streamType);

        for(auto &effProfile: effectiveStreamProfiles_) {
            if(isMatch(sensorType, videoProfile, effProfile)) {
                filteredProfiles.push_back(profile);
                continue;
            }
        }
    }

    return filteredProfiles;
}
void G2StreamProfileFilter::fetchEffectiveStreamProfiles() {
    auto owner      = getOwner();
    auto propServer = owner->getPropertyServer();

    std::vector<OBEffectiveStreamProfile> profiles;
    profiles                 = propServer->getStructureDataListProtoV1_1_T<OBEffectiveStreamProfile, 0>(OB_RAW_DATA_EFFECTIVE_VIDEO_STREAM_PROFILE_LIST);
    effectiveStreamProfiles_ = profiles;
    for(auto &profile: profiles) {
        LOG_DEBUG("effective stream profile: sensorType: {}, format: {}, width: {}, height: {}, maxFps: {}", profile.sensorType, profile.format,
                  profile.width, profile.height, profile.maxFps);
        if(profile.sensorType == OB_SENSOR_COLOR && profile.format == OB_FORMAT_MJPG) {
            auto newProfile   = profile;
            newProfile.format = OB_FORMAT_RGB;
            effectiveStreamProfiles_.push_back(newProfile);
            newProfile.format = OB_FORMAT_BGR;
            effectiveStreamProfiles_.push_back(newProfile);
            newProfile.format = OB_FORMAT_BGRA;
            effectiveStreamProfiles_.push_back(newProfile);
        }
        else if(profile.sensorType == OB_SENSOR_DEPTH && profile.format == OB_FORMAT_RLE) {
            auto newProfile   = profile;
            newProfile.format = OB_FORMAT_Y16;
            auto iter         = std::find_if(profiles.begin(), profiles.end(), [&newProfile](const OBEffectiveStreamProfile &profile) {
                return profile.sensorType == newProfile.sensorType && profile.format == OB_FORMAT_Y16 && profile.width == newProfile.width
                       && profile.height == newProfile.height && profile.maxFps == newProfile.maxFps;
            });

            if(iter == profiles.end()) {
                effectiveStreamProfiles_.push_back(newProfile);
            }
        }
    }
}

}  // namespace libobsensor
