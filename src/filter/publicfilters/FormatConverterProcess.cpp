// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "FormatConverterProcess.hpp"
#include "exception/ObException.hpp"
#include "logger/LoggerInterval.hpp"
#include "frame/FrameFactory.hpp"
#include "frame/FrameMemoryPool.hpp"
#include "stream/StreamProfile.hpp"
#include "libobsensor/h/ObTypes.h"
#include <libyuv.h>
#include <turbojpeg.h>

namespace libobsensor {

FormatConverter::FormatConverter() : convertType_(FORMAT_YUYV_TO_RGB) {}
FormatConverter::~FormatConverter() noexcept {}

void FormatConverter::updateConfig(std::vector<std::string> &params) {
    if(params.size() != 1) {
        throw invalid_value_exception("FormatConverter config error: params size not match");
    }
    try {
        int convertType = std::stoi(params[0]);
        convertType_    = (OBConvertFormat)convertType;
    }
    catch(const std::exception &e) {
        throw invalid_value_exception("FormatConverter config error: " + std::string(e.what()));
    }
}

const std::string &FormatConverter::getConfigSchema() const {
    // csv format: name，type， min，max，step，default，description
    static const std::string schema = "convertType, int, 0, 21, 1, 0, frame data converter type";
    return schema;
}

void FormatConverter::reset() {
    currentStreamProfile_.reset();
    tarStreamProfile_.reset();
}

const std::map<OBConvertFormat, std::pair<OBFormat, OBFormat>> FORMAT_CONVERT_MAP = {
    { FORMAT_YUYV_TO_RGB, { OB_FORMAT_YUYV, OB_FORMAT_RGB } },   { FORMAT_I420_TO_RGB, { OB_FORMAT_I420, OB_FORMAT_RGB } },
    { FORMAT_NV21_TO_RGB, { OB_FORMAT_NV21, OB_FORMAT_RGB } },   { FORMAT_NV12_TO_RGB, { OB_FORMAT_NV12, OB_FORMAT_RGB } },
    { FORMAT_MJPG_TO_I420, { OB_FORMAT_MJPG, OB_FORMAT_I420 } }, { FORMAT_RGB_TO_BGR, { OB_FORMAT_RGB, OB_FORMAT_BGR } },
    { FORMAT_MJPG_TO_NV21, { OB_FORMAT_MJPG, OB_FORMAT_NV21 } }, { FORMAT_MJPG_TO_RGB, { OB_FORMAT_MJPG, OB_FORMAT_RGB } },
    { FORMAT_MJPG_TO_BGR, { OB_FORMAT_MJPG, OB_FORMAT_BGR } },   { FORMAT_MJPG_TO_BGRA, { OB_FORMAT_MJPG, OB_FORMAT_BGRA } },
    { FORMAT_UYVY_TO_RGB, { OB_FORMAT_UYVY, OB_FORMAT_RGB } },   { FORMAT_BGR_TO_RGB, { OB_FORMAT_BGR, OB_FORMAT_RGB } },
    { FORMAT_MJPG_TO_NV12, { OB_FORMAT_MJPG, OB_FORMAT_NV12 } }, { FORMAT_YUYV_TO_BGR, { OB_FORMAT_YUYV, OB_FORMAT_BGR } },
    { FORMAT_YUYV_TO_RGBA, { OB_FORMAT_YUYV, OB_FORMAT_RGBA } }, { FORMAT_YUYV_TO_BGRA, { OB_FORMAT_YUYV, OB_FORMAT_BGRA } },
    { FORMAT_YUYV_TO_Y16, { OB_FORMAT_YUYV, OB_FORMAT_Y16 } },   { FORMAT_YUYV_TO_Y8, { OB_FORMAT_YUYV, OB_FORMAT_Y8 } },
    { FORMAT_RGBA_TO_RGB, { OB_FORMAT_RGBA, OB_FORMAT_RGB } },   { FORMAT_BGRA_TO_BGR, { OB_FORMAT_BGRA, OB_FORMAT_BGR } },
    { FORMAT_Y16_TO_RGB, { OB_FORMAT_Y16, OB_FORMAT_RGB } },     { FORMAT_Y8_TO_RGB, { OB_FORMAT_Y8, OB_FORMAT_RGB } },
};

void FormatConverter::setConversion(OBFormat srcFormat, OBFormat dstFormat) {
    for(auto &item: FORMAT_CONVERT_MAP) {
        if(item.second.first == srcFormat && item.second.second == dstFormat) {
            convertType_ = item.first;
            currentStreamProfile_.reset();
            tarStreamProfile_.reset();
            return;
        }
    }
    throw invalid_value_exception("FormatConverter config error: invalid format conversion");
}

std::shared_ptr<Frame> FormatConverter::process(std::shared_ptr<const Frame> frame) {
    if(!frame) {
        return nullptr;
    }

    auto videoFrame = frame->as<VideoFrame>();
    int  w          = videoFrame->getWidth();
    int  h          = videoFrame->getHeight();

    auto streamprofile = frame->getStreamProfile();
    if(!currentStreamProfile_ || currentStreamProfile_.get() != streamprofile.get()) {
        currentStreamProfile_ = streamprofile;
        tarStreamProfile_     = streamprofile->clone();
        if(FORMAT_CONVERT_MAP.find(convertType_) == FORMAT_CONVERT_MAP.end()) {
            auto srcFormat = streamprofile->getFormat();
            auto dstFormat = OB_FORMAT_RGB;
            for(auto &item: FORMAT_CONVERT_MAP) {
                if(item.second.first == srcFormat) {
                    dstFormat = item.second.second;
                    tarStreamProfile_->setFormat(dstFormat);
                    break;
                }
            }
        }
        else {
            tarStreamProfile_->setFormat(FORMAT_CONVERT_MAP.at(convertType_).second);
        }
    }

    auto tarFrame = FrameFactory::createFrameFromStreamProfile(tarStreamProfile_);
    if(tarFrame == nullptr) {
        LOG_ERROR_INTVL("Create frame by frame factory failed!");
        return nullptr;
    }

    tarFrame->copyInfoFromOther(frame);
    switch(convertType_) {
    case FORMAT_YUYV_TO_RGB:
        yuyvToRgb((uint8_t *)frame->getData(), (uint8_t *)tarFrame->getData(), w, h);
        break;
    case FORMAT_YUYV_TO_RGBA:
        yuyvToRgba((uint8_t *)frame->getData(), (uint8_t *)tarFrame->getData(), w, h);
        break;
    case FORMAT_YUYV_TO_BGR:
        yuyvToBgr((uint8_t *)frame->getData(), (uint8_t *)tarFrame->getData(), w, h);
        break;
    case FORMAT_YUYV_TO_BGRA:
        yuyvToBgra((uint8_t *)frame->getData(), (uint8_t *)tarFrame->getData(), w, h);
        break;
    case FORMAT_YUYV_TO_Y16:
        yuyvToy16((uint8_t *)frame->getData(), (uint8_t *)tarFrame->getData(), w, h);
        break;
    case FORMAT_YUYV_TO_Y8:
        yuyvToy8((uint8_t *)frame->getData(), (uint8_t *)tarFrame->getData(), w, h);
        break;
    case FORMAT_UYVY_TO_RGB:
        uyvyToRgb((uint8_t *)frame->getData(), (uint8_t *)tarFrame->getData(), w, h);
        break;
    case FORMAT_I420_TO_RGB:
        i420ToRgb((uint8_t *)frame->getData(), (uint8_t *)tarFrame->getData(), w, h);
        break;
    case FORMAT_NV21_TO_RGB:
        nv21ToRgb((uint8_t *)frame->getData(), (uint8_t *)tarFrame->getData(), w, h);
        break;
    case FORMAT_NV12_TO_RGB:
        nv12ToRgb((uint8_t *)frame->getData(), (uint8_t *)tarFrame->getData(), w, h);
        break;
    case FORMAT_MJPG_TO_I420:
        mjpgToI420((uint8_t *)frame->getData(), (uint32_t)frame->getDataSize(), (uint8_t *)tarFrame->getData(), w, h);
        break;
    case FORMAT_RGB_TO_BGR:
        exchangeRAndB((uint8_t *)frame->getData(), (uint8_t *)tarFrame->getData(), w, h);
        break;
    case FORMAT_BGR_TO_RGB:
        exchangeRAndB((uint8_t *)frame->getData(), (uint8_t *)tarFrame->getData(), w, h);
        break;
    case FORMAT_MJPG_TO_NV21:
        mjpgToNv21((uint8_t *)frame->getData(), (uint32_t)frame->getDataSize(), (uint8_t *)tarFrame->getData(), w, h);
        break;
    case FORMAT_MJPG_TO_RGB:
        if(!mjpgToRgb((uint8_t *)frame->getData(), (uint32_t)frame->getDataSize(), (uint8_t *)tarFrame->getData(), w, h))
            return nullptr;
        break;
    case FORMAT_MJPG_TO_BGR:
        mjpgToBgr((uint8_t *)frame->getData(), (uint32_t)frame->getDataSize(), (uint8_t *)tarFrame->getData(), w, h);
        break;
    case FORMAT_MJPG_TO_BGRA:
        mjpegToBgra((uint8_t *)frame->getData(), (uint32_t)frame->getDataSize(), (uint8_t *)tarFrame->getData(), w, h);
        break;
    case FORMAT_MJPG_TO_NV12:
        mjpgToNv12((uint8_t *)frame->getData(), (uint32_t)frame->getDataSize(), (uint8_t *)tarFrame->getData(), w, h);
        break;
    case FORMAT_RGBA_TO_RGB:
        rgbaToRgb((uint8_t *)frame->getData(), (uint32_t)frame->getDataSize(), (uint8_t *)tarFrame->getData(), w, h);
        break;
    case FORMAT_BGRA_TO_BGR:
        bgraToBgr((uint8_t *)frame->getData(), (uint32_t)frame->getDataSize(), (uint8_t *)tarFrame->getData(), w, h);
        break;
    case FORMAT_Y16_TO_RGB:
        y16ToRgb((uint8_t *)frame->getData(), (uint32_t)frame->getDataSize(), (uint8_t *)tarFrame->getData(), w, h);
        break;
    case FORMAT_Y8_TO_RGB:
        y8ToRgb((uint8_t *)frame->getData(), (uint32_t)frame->getDataSize(), (uint8_t *)tarFrame->getData(), w, h);
        break;
    default:
        LOG_WARN_INTVL("Unsupported data format conversion.");
        return FrameFactory::createFrameFromOtherFrame(frame, true);
        break;
    }
    return tarFrame;
}

void FormatConverter::yuyvToRgb(uint8_t *src, uint8_t *target, uint32_t width, uint32_t height) {
    const auto preferSize = width * height * 3 / 2;
    if(tempDataBuf_ == nullptr || preferSize != tempDataBufSize_) {
        if(nullptr != tempDataBuf_) {
            delete[] tempDataBuf_;
        }
        tempDataBuf_     = new uint8_t[preferSize];
        tempDataBufSize_ = preferSize;
    }
    uint8_t *yData = tempDataBuf_;
    uint8_t *uData = tempDataBuf_ + width * height;
    uint8_t *vData = tempDataBuf_ + width * height * 5 / 4;
    libyuv::YUY2ToI420(src, width * 2, yData, width, uData, width / 2, vData, width / 2, width, height);
    libyuv::I420ToRAW(yData, width, uData, width / 2, vData, width / 2, target, width * 3, width, height);
}

void FormatConverter::yuyvToRgba(uint8_t *src, uint8_t *target, uint32_t width, uint32_t height) {
    const auto preferSize = width * height * 4;
    if(tempDataBuf_ == nullptr || preferSize != tempDataBufSize_) {
        if(nullptr != tempDataBuf_) {
            delete[] tempDataBuf_;
        }
        tempDataBuf_     = new uint8_t[preferSize];
        tempDataBufSize_ = preferSize;
    }
    uint8_t *yData = tempDataBuf_;
    uint8_t *uData = tempDataBuf_ + width * height;
    uint8_t *vData = tempDataBuf_ + width * height * 5 / 4;
    libyuv::YUY2ToI420(src, width * 2, yData, width, uData, width / 2, vData, width / 2, width, height);
    libyuv::I420ToABGR(yData, width, uData, width / 2, vData, width / 2, target, width * 4, width, height);
}

void FormatConverter::yuyvToBgr(uint8_t *src, uint8_t *target, uint32_t width, uint32_t height) {
    const auto preferSize = width * height * 3 / 2;
    if(tempDataBuf_ == nullptr || preferSize != tempDataBufSize_) {
        if(nullptr != tempDataBuf_) {
            delete[] tempDataBuf_;
        }
        tempDataBuf_     = new uint8_t[preferSize];
        tempDataBufSize_ = preferSize;
    }
    uint8_t *yData = tempDataBuf_;
    uint8_t *uData = tempDataBuf_ + width * height;
    uint8_t *vData = tempDataBuf_ + width * height * 5 / 4;
    libyuv::YUY2ToI420(src, width * 2, yData, width, uData, width / 2, vData, width / 2, width, height);
    libyuv::I420ToRGB24(yData, width, uData, width / 2, vData, width / 2, target, width * 3, width, height);
}

void FormatConverter::yuyvToBgra(uint8_t *src, uint8_t *target, uint32_t width, uint32_t height) {
    const auto preferSize = width * height * 4;
    if(tempDataBuf_ == nullptr || preferSize != tempDataBufSize_) {
        if(nullptr != tempDataBuf_) {
            delete[] tempDataBuf_;
        }
        tempDataBuf_     = new uint8_t[preferSize];
        tempDataBufSize_ = preferSize;
    }
    uint8_t *yData = tempDataBuf_;
    uint8_t *uData = tempDataBuf_ + width * height;
    uint8_t *vData = tempDataBuf_ + width * height * 5 / 4;
    libyuv::YUY2ToI420(src, width * 2, yData, width, uData, width / 2, vData, width / 2, width, height);
    libyuv::I420ToARGB(yData, width, uData, width / 2, vData, width / 2, target, width * 4, width, height);
}

void FormatConverter::yuyvToy16(uint8_t *src, uint8_t *target, uint32_t width, uint32_t height) {
    auto size = width * height;
    for(uint32_t i = 0; i < size; i += 16) {
        uint8_t tarArray[32] = {
            0, src[i * 2],      0, src[i * 2 + 2],  0, src[i * 2 + 4],  0, src[i * 2 + 6],  0, src[i * 2 + 8],  0, src[i * 2 + 10],
            0, src[i * 2 + 12], 0, src[i * 2 + 14], 0, src[i * 2 + 16], 0, src[i * 2 + 18], 0, src[i * 2 + 20], 0, src[i * 2 + 22],
            0, src[i * 2 + 24], 0, src[i * 2 + 26], 0, src[i * 2 + 28], 0, src[i * 2 + 30],
        };
        memcpy(target + i * 2, tarArray, 32);
    }
}

void FormatConverter::yuyvToy8(uint8_t *src, uint8_t *target, uint32_t width, uint32_t height) {
    auto size = width * height;
    for(uint32_t i = 0; i < size; i += 16) {
        target[i]      = src[i * 2];
        target[i + 1]  = src[i * 2 + 2];
        target[i + 2]  = src[i * 2 + 4];
        target[i + 3]  = src[i * 2 + 6];
        target[i + 4]  = src[i * 2 + 8];
        target[i + 5]  = src[i * 2 + 10];
        target[i + 6]  = src[i * 2 + 12];
        target[i + 7]  = src[i * 2 + 14];
        target[i + 8]  = src[i * 2 + 16];
        target[i + 9]  = src[i * 2 + 18];
        target[i + 10] = src[i * 2 + 20];
        target[i + 11] = src[i * 2 + 22];
        target[i + 12] = src[i * 2 + 24];
        target[i + 13] = src[i * 2 + 26];
        target[i + 14] = src[i * 2 + 28];
        target[i + 15] = src[i * 2 + 30];
    }
}

void FormatConverter::uyvyToRgb(uint8_t *src, uint8_t *target, uint32_t width, uint32_t height) {
    const auto preferSize = width * height * 3 / 2;
    if(tempDataBuf_ == nullptr || preferSize != tempDataBufSize_) {
        if(nullptr != tempDataBuf_) {
            delete[] tempDataBuf_;
        }
        tempDataBuf_     = new uint8_t[preferSize];
        tempDataBufSize_ = preferSize;
    }
    uint8_t *yData = tempDataBuf_;
    uint8_t *uData = tempDataBuf_ + width * height;
    uint8_t *vData = tempDataBuf_ + width * height * 5 / 4;
    libyuv::UYVYToI420(src, width * 2, yData, width, uData, width / 2, vData, width / 2, width, height);
    libyuv::I420ToRAW(yData, width, uData, width / 2, vData, width / 2, target, width * 3, width, height);
}

void FormatConverter::i420ToRgb(uint8_t *src, uint8_t *target, uint32_t width, uint32_t height) {
    uint8_t *yData = src;
    uint8_t *uData = src + width * height;
    uint8_t *vData = src + width * height * 5 / 4;
    libyuv::I420ToRAW(yData, width, uData, width / 2, vData, width / 2, target, width * 3, width, height);
}

void FormatConverter::nv21ToRgb(uint8_t *src, uint8_t *target, uint32_t width, uint32_t height) {
    uint8_t *yData  = src;
    uint8_t *vuData = src + width * height;
    libyuv::NV21ToRAW(yData, width, vuData, width, target, width * 3, width, height);
}

void FormatConverter::nv12ToRgb(uint8_t *src, uint8_t *target, uint32_t width, uint32_t height) {
    uint8_t *yData  = src;
    uint8_t *vuData = src + width * height;
    libyuv::NV12ToRAW(yData, width, vuData, width, target, width * 3, width, height);
}

void FormatConverter::mjpgToI420(uint8_t *src, uint32_t src_len, uint8_t *target, uint32_t width, uint32_t height) {
    if(src == nullptr || target == nullptr) {
        LOG_ERROR_INTVL("FormatConverter mjpegFrame is null or dstFrame is null");
        return;
    }

    uint8_t *yData = target;
    uint8_t *uData = target + width * height;
    uint8_t *vData = target + width * height * 5 / 4;

    int ret = libyuv::MJPGToI420(src, src_len, yData, width, uData, width / 2, vData, width / 2, width, height, width, height);
    if(ret != 0) {
        LOG_ERROR_INTVL("mjpeg to yuv error");
    }
}

void FormatConverter::mjpgToNv21(uint8_t *src, uint32_t src_len, uint8_t *target, uint32_t width, uint32_t height) {
    int ret;

    uint8_t *yData  = target;
    uint8_t *vuData = target + width * height;

    ret = libyuv::MJPGToNV21(src, src_len, yData, width, vuData, width, width, height, width, height);
    if(ret != 0) {
        LOG_ERROR_INTVL("mjpeg to nv21 error");
    }
}

void FormatConverter::mjpgToNv12(uint8_t *src, uint32_t src_len, uint8_t *target, uint32_t width, uint32_t height) {
    int ret;

    uint8_t *yData  = target;
    uint8_t *vuData = target + width * height;

    ret = libyuv::MJPGToNV12(src, src_len, yData, width, vuData, width, width, height, width, height);
    if(ret != 0) {
        LOG_ERROR_INTVL("mjpeg to nv12 error");
    }
}

bool FormatConverter::mjpgToRgb(uint8_t *src, uint32_t src_len, uint8_t *target, uint32_t width, uint32_t height) {
    tjhandle tjHandle;
    tjHandle = tjInitDecompress();
    if(tjDecompress2(tjHandle, src, src_len, target, width,
                     0,  // pitch
                     height, TJPF_RGB, TJFLAG_FASTDCT | TJFLAG_FASTUPSAMPLE)
       != 0) {
        LOG_WARN_INTVL("Failed to decode mjpeg frame to rgb! {}", tjGetErrorStr2(tjHandle));
        tjDestroy(tjHandle);
        return false;
    }
    tjDestroy(tjHandle);
    return true;
}

bool FormatConverter::mjpgToBgr(uint8_t *src, uint32_t src_len, uint8_t *target, uint32_t width, uint32_t height) {
    tjhandle tjHandle;
    tjHandle = tjInitDecompress();
    if(tjDecompress2(tjHandle, src, src_len, target, width,
                     0,  // pitch
                     height, TJPF_BGR, TJFLAG_FASTDCT | TJFLAG_FASTUPSAMPLE)
       != 0) {
        LOG_WARN_INTVL("Failed to decode mjpeg frame to bgr! {}", tjGetErrorStr2(tjHandle));
        tjDestroy(tjHandle);
        return false;
    }
    tjDestroy(tjHandle);
    return true;
}

void FormatConverter::exchangeRAndB(uint8_t *pucRgb, uint8_t *target, uint32_t width, uint32_t height, uint32_t pixelSize) {
    if(pucRgb == nullptr || target == nullptr) {
        return;
    }
    for(uint32_t i = 0; i < height; i++) {
        for(uint32_t j = 0; j < width; j++) {
            uint8_t tmp1                                      = pucRgb[i * width * pixelSize + j * pixelSize];
            target[i * width * pixelSize + j * pixelSize]     = pucRgb[i * width * pixelSize + j * pixelSize + 2];
            target[i * width * pixelSize + j * pixelSize + 1] = pucRgb[i * width * pixelSize + j * pixelSize + 1];
            target[i * width * pixelSize + j * pixelSize + 2] = tmp1;
        }
    }
}

void FormatConverter::mjpegToBgra(uint8_t *src, uint32_t src_len, uint8_t *target, uint32_t width, uint32_t height) {
    tjhandle tjHandle;
    tjHandle = tjInitDecompress();
    if(tjDecompress2(tjHandle, src, src_len, target, width,
                     0,  // pitch
                     height, TJPF_BGRA, TJFLAG_FASTDCT | TJFLAG_FASTUPSAMPLE)
       != 0) {
        LOG_WARN_INTVL("Failed to decompress color frame");
    }
    tjDestroy(tjHandle);
}

void FormatConverter::rgbaToRgb(uint8_t *src, uint32_t src_len, uint8_t *target, uint32_t width, uint32_t height) {
    if(src == nullptr || target == nullptr)
        return;

    uint32_t expected_rgba_len = width * height * 4;
    if(src_len < expected_rgba_len)
        return;

    for(uint32_t i = 0; i < width * height; ++i) {
        target[i * 3]     = src[i * 4];      // R
        target[i * 3 + 1] = src[i * 4 + 1];  // G
        target[i * 3 + 2] = src[i * 4 + 2];  // B
    }
}

void FormatConverter::bgraToBgr(uint8_t *src, uint32_t src_len, uint8_t *target, uint32_t width, uint32_t height){
    if(src == nullptr || target == nullptr)
        return;

    uint32_t expected_bgra_len = width * height * 4;
    if(src_len < expected_bgra_len)
        return;

    for(uint32_t i = 0; i < width * height; ++i) {
        target[i * 3]     = src[i * 4];      // B
        target[i * 3 + 1] = src[i * 4 + 1];  // G
        target[i * 3 + 2] = src[i * 4 + 2];  // R
    }
}

void FormatConverter::y16ToRgb(uint8_t *src, uint32_t src_len, uint8_t *target, uint32_t width, uint32_t height) {
    if(src == nullptr || target == nullptr)
        return;

    const uint32_t pixel_count      = width * height;
    const uint32_t expected_src_len = pixel_count * 2;
    if(src_len < expected_src_len)
        return;

    const uint16_t *y16_data = reinterpret_cast<const uint16_t *>(src);

    for(uint32_t i = 0; i < pixel_count; ++i) {
        uint8_t y8 = y16_data[i] >> 8;

        target[i * 3]     = y8;  // R
        target[i * 3 + 1] = y8;  // G
        target[i * 3 + 2] = y8;  // B
    }
}

void FormatConverter::y8ToRgb(uint8_t *src, uint32_t src_len, uint8_t *target, uint32_t width, uint32_t height) {
    if(src == nullptr || target == nullptr)
        return;

    const uint32_t pixel_count = width * height;
    if(src_len < pixel_count)
        return;

    for(uint32_t i = 0; i < pixel_count; ++i) {
        const uint8_t y = src[i];

        target[i * 3]     = y;  // R
        target[i * 3 + 1] = y;  // G
        target[i * 3 + 2] = y;  // B
    }
}


}  // namespace libobsensor

