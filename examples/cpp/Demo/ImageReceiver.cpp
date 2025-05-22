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
#include "window.hpp"

const char *metaDataTypes[] = {"TIMESTAMP",
                               "SENSOR_TIMESTAMP",
                               "FRAME_NUMBER",
                               "AUTO_EXPOSURE",
                               "EXPOSURE",
                               "GAIN",
                               "AUTO_WHITE_BALANCE",
                               "WHITE_BALANCE",
                               "BRIGHTNESS",
                               "CONTRAST",
                               "SATURATION",
                               "SHARPNESS",
                               "BACKLIGHT_COMPENSATION",
                               "HUE",
                               "GAMMA",
                               "POWER_LINE_FREQUENCY",
                               "LOW_LIGHT_COMPENSATION",
                               "MANUAL_WHITE_BALANCE",
                               "ACTUAL_FRAME_RATE",
                               "FRAME_RATE",
                               "AE_ROI_LEFT",
                               "AE_ROI_TOP",
                               "AE_ROI_RIGHT",
                               "AE_ROI_BOTTOM",
                               "EXPOSURE_PRIORITY",
                               "HDR_SEQUENCE_NAME",
                               "HDR_SEQUENCE_SIZE",
                               "HDR_SEQUENCE_INDEX",
                               "LASER_POWER",
                               "LASER_POWER_LEVEL",
                               "LASER_STATUS",
                               "GPIO_INPUT_DATA"};

// 全局配置结构体
class StreamConfig {
public:
    static StreamConfig& getInstance() {
        static StreamConfig instance;
        return instance;
    }

    // 删除拷贝构造和赋值操作
    StreamConfig(const StreamConfig&) = delete;
    StreamConfig& operator=(const StreamConfig&) = delete;

    // 数据流配置
    bool enableColor = true;
    bool enableDepth = true;
    bool enableIR = false;
    bool enableIRLeft = false;
    bool enableIRRight = false;
    bool enableIMU = false;

    // 渲染配置
    bool enableRendering = true;
    int windowWidth = 1280;
    int windowHeight = 720;

    // 数据保存配置
    bool enableDump = false;
    std::string dumpPath = "./";

    // 元数据显示配置
    bool enableMetadata = false;
    int metadataPrintInterval = 30;  // 每30帧打印一次元数据

private:
    // 私有构造函数
    StreamConfig() = default;
};

OBStreamType SensorTypeToStreamType(OBSensorType sensorType) {
  switch (sensorType) {
    case OB_SENSOR_COLOR:
      return OB_STREAM_COLOR;
    case OB_SENSOR_DEPTH:
      return OB_STREAM_DEPTH;
    case OB_SENSOR_IR:
      return OB_STREAM_IR;
    case OB_SENSOR_IR_LEFT:
      return OB_STREAM_IR_LEFT;
    case OB_SENSOR_IR_RIGHT:
      return OB_STREAM_IR_RIGHT;
    case OB_SENSOR_GYRO:
      return OB_STREAM_GYRO;
    case OB_SENSOR_ACCEL:
      return OB_STREAM_ACCEL;
    default:
      return OB_STREAM_UNKNOWN;
  }
}

class ImageReceiver {
 public:
  ImageReceiver();
  ~ImageReceiver();

  bool Init();
  void Run();

 private:
  void ProcessFrameSet(std::shared_ptr<ob::FrameSet> frameset);
  void ProcessFrame(std::shared_ptr<ob::Frame> frame);
  void SaveFrame(std::shared_ptr<ob::Frame> frame);
  void PrintMetadata(std::shared_ptr<ob::Frame> frame);
  void HandleError(ob::Error &e);
  void Cleanup();  // 统一的资源清理函数

  // 辅助函数
  std::string GetFrameTypeString(OBFrameType type);
  std::string GetTimestampString();
  bool IsVideoFrame(OBFrameType type);
  bool IsIMUFrame(OBFrameType type);

 private:
  ob::Context ctx;
  std::shared_ptr<ob::Device> dev;
  std::shared_ptr<ob::DeviceInfo> devInfo;

  // 统一的数据流pipeline
  ob::Pipeline pipe_;
  std::shared_ptr<ob::Config> config_;

  Window app;
  ob::FormatConvertFilter formatConvertFilter_;

  int kMaxMetadataNameLength_;

  std::mutex frameMutex_;
  std::map<OBFrameType, std::shared_ptr<ob::Frame>> frameMap_;
};

ImageReceiver::ImageReceiver()
    : app("MultiStream", StreamConfig::getInstance().windowWidth,
          StreamConfig::getInstance().windowHeight, RENDER_GRID) {
  Init();
}

bool ImageReceiver::Init() {
  try {
    // 1. 配置数据流
    config_ = std::make_shared<ob::Config>();
    auto device = pipe_.getDevice();
    if (!device) {
      std::cerr << "Failed to get device" << std::endl;
      return false;
    }

    auto sensorList = device->getSensorList();
    if (!sensorList) {
      std::cerr << "Failed to get sensor list" << std::endl;
      return false;
    }

    // 配置视频流
    for (uint32_t i = 0; i < sensorList->count(); ++i) {
      auto sensorType = sensorList->type(i);
      auto streamType = SensorTypeToStreamType(sensorType);

      // 根据配置启用相应的数据流
      if ((streamType == OB_STREAM_COLOR && StreamConfig::getInstance().enableColor) ||
          (streamType == OB_STREAM_DEPTH && StreamConfig::getInstance().enableDepth) ||
          (streamType == OB_STREAM_IR && StreamConfig::getInstance().enableIR) ||
          (streamType == OB_STREAM_IR_LEFT && StreamConfig::getInstance().enableIRLeft) ||
          (streamType == OB_STREAM_IR_RIGHT && StreamConfig::getInstance().enableIRRight)) {
        config_->enableVideoStream(streamType);
      }

      // 配置IMU数据流
      if (StreamConfig::getInstance().enableIMU) {
        if (sensorType == OB_SENSOR_GYRO) {
          config_->enableGyroStream();
        } else if (sensorType == OB_SENSOR_ACCEL) {
          config_->enableAccelStream();
        }
      }
    }

    // 2. 初始化元数据长度
    kMaxMetadataNameLength_ = []() {
      size_t max = 0;
      for (uint32_t i = 0; i < OB_FRAME_METADATA_TYPE_COUNT; ++i) {
        size_t len = strlen(metaDataTypes[i]);
        if (len > max) max = len;
      }
      return max;
    }();

    return true;
  } catch (ob::Error &e) {
    HandleError(e);
    return false;
  }
}

void ImageReceiver::ProcessFrameSet(std::shared_ptr<ob::FrameSet> frameset) {
  if (!frameset) return;

  std::unique_lock<std::mutex> lk(frameMutex_);
  for (uint32_t i = 0; i < frameset->frameCount(); ++i) {
    auto frame = frameset->getFrame(i);
    frameMap_[frame->type()] = frame;

    // 处理单个帧
    ProcessFrame(frame);
  }
}

void ImageReceiver::ProcessFrame(std::shared_ptr<ob::Frame> frame) {
  if (!frame) return;

  // 打印元数据
  if (StreamConfig::getInstance().enableMetadata &&
      frame->index() % StreamConfig::getInstance().metadataPrintInterval == 0) {
    PrintMetadata(frame);
  }

  // 保存帧数据
  if (StreamConfig::getInstance().enableDump) {
    SaveFrame(frame);
  }
}

void ImageReceiver::Run() {
  try {
    // 1. 启动数据流
    pipe_.start(config_, [this](std::shared_ptr<ob::FrameSet> frameset) {
      ProcessFrameSet(frameset);
    });

    // 2. 主循环
    while (app) {
      std::vector<std::shared_ptr<ob::Frame>> framesForRender;

      // 收集所有帧用于渲染
      {
        std::unique_lock<std::mutex> lock(frameMutex_);
        for (auto &frame : frameMap_) {
          framesForRender.push_back(frame.second);
        }
      }

      // 渲染帧
      if (StreamConfig::getInstance().enableRendering) {
        app.addToRender(framesForRender);
      }

      // 检查按键和窗口关闭
      int key = app.waitKey(30);
      if (key == ESC_KEY) {
        std::cout << "ESC key pressed, exiting..." << std::endl;
        break;
      }
    }

    // 3. 清理资源
    Cleanup();
  } catch (ob::Error &e) {
    HandleError(e);
    Cleanup();  // 确保在错误情况下也清理资源
  } catch (const std::exception &e) {
    std::cerr << "Runtime error: " << e.what() << std::endl;
    Cleanup();  // 确保在错误情况下也清理资源
  }
}

void ImageReceiver::PrintMetadata(std::shared_ptr<ob::Frame> frame) {
  if (!frame) return;

  std::ostringstream oss;
  const int padding = 4;
  const size_t lineLength = kMaxMetadataNameLength_ + padding * 2 + 2;
  std::string line(lineLength, '*');

  oss << line << "\n"
      << GetFrameTypeString(frame->type()) << " Frame #" << frame->index()
      << " Metadata List\n"
      << line << "\n";

  for (int type = 0; type < OB_FRAME_METADATA_TYPE_COUNT; type++) {
    if (frame->hasMetadata((OBFrameMetadataType)type)) {
      oss << std::left << std::setw(kMaxMetadataNameLength_)
          << metaDataTypes[type] << ": "
          << frame->getMetadataValue((OBFrameMetadataType)type) << "\n";
    } else {
      oss << std::left << std::setw(kMaxMetadataNameLength_)
          << metaDataTypes[type] << ": unsupported\n";
    }
  }

  oss << line << "\n\n";
  std::cout << oss.str();
}

void ImageReceiver::SaveFrame(std::shared_ptr<ob::Frame> frame) {
  if (!StreamConfig::getInstance().enableDump || !frame) return;

  try {
    std::string frameType = GetFrameTypeString(frame->type());
    std::string timestamp = GetTimestampString();

    // 构建文件名
    std::ostringstream oss;

    // 根据帧类型进行不同的处理
    if (IsVideoFrame(frame->type())) {
      auto videoFrame = frame->as<ob::VideoFrame>();
      if (videoFrame) {
        oss << StreamConfig::getInstance().dumpPath << frameType << "_"
            << videoFrame->width() << "x" << videoFrame->height() << "_"
            << "F" << frame->index() << "_"
            << "T" << frame->timeStamp() << "ms_" << timestamp << ".png";

        // 处理视频帧
        cv::Mat frameMat;
        if (frame->type() == OB_FRAME_COLOR) {
          auto colorFrame = videoFrame->as<ob::ColorFrame>();
          if (colorFrame) {
            // 颜色帧处理
            if (colorFrame->format() != OB_FORMAT_RGB) {
              formatConvertFilter_.setFormatConvertType(
                  colorFrame->format() == OB_FORMAT_MJPG ? FORMAT_MJPG_TO_RGB888
                  : colorFrame->format() == OB_FORMAT_UYVY
                      ? FORMAT_UYVY_TO_RGB888
                  : colorFrame->format() == OB_FORMAT_YUYV
                      ? FORMAT_YUYV_TO_RGB888
                      : throw std::runtime_error("Unsupported color format"));
              colorFrame = formatConvertFilter_.process(colorFrame)
                               ->as<ob::ColorFrame>();
            }
            frameMat = cv::Mat(colorFrame->height(), colorFrame->width(),
                               CV_8UC3, colorFrame->data());
          }
        } else {
          // 深度/IR帧处理
          frameMat = cv::Mat(videoFrame->height(), videoFrame->width(),
                             CV_16UC1, videoFrame->data());
        }

        // 保存图像
        std::vector<int> compressionParams = {cv::IMWRITE_PNG_COMPRESSION, 0,
                                              cv::IMWRITE_PNG_STRATEGY,
                                              cv::IMWRITE_PNG_STRATEGY_DEFAULT};

        if (cv::imwrite(oss.str(), frameMat, compressionParams)) {
          std::cout << "Saved frame to: " << oss.str() << std::endl;
        }
      }
    } else if (IsIMUFrame(frame->type())) {
      // IMU数据保存
      oss << StreamConfig::getInstance().dumpPath << frameType << "_"
          << "F" << frame->index() << "_"
          << "T" << frame->timeStamp() << "ms_" << timestamp << ".txt";

      // 保存IMU数据到文本文件
      std::ofstream file(oss.str());
      if (file.is_open()) {
        file << "Frame Type: " << frameType << "\n"
             << "Frame Index: " << frame->index() << "\n"
             << "Timestamp: " << frame->timeStamp() << "ms\n";

        // 根据IMU类型处理数据
        if (frame->type() == OB_FRAME_GYRO) {
          auto gyroFrame = frame->as<ob::GyroFrame>();
          if (gyroFrame) {
            void *rawData = gyroFrame->data();
            if (rawData) {
              float *gyroData = reinterpret_cast<float *>(rawData);
              file << "Gyro Data:\n"
                   << " X: " << gyroData[0] << "\n"
                   << " Y: " << gyroData[1] << "\n"
                   << " Z: " << gyroData[2] << "\n";
            }
          }
        } else if (frame->type() == OB_FRAME_ACCEL) {
          auto accelFrame = frame->as<ob::AccelFrame>();
          if (accelFrame) {
            void *rawData = accelFrame->data();
            if (rawData) {
              float *accelData = reinterpret_cast<float *>(rawData);
              file << "Accel Data:\n"
                   << " X: " << accelData[0] << "\n"
                   << " Y: " << accelData[1] << "\n"
                   << " Z: " << accelData[2] << "\n";
            }
          }
        }

        file.close();
        std::cout << "Saved IMU data to: " << oss.str() << std::endl;
      }
    }
  } catch (const std::exception &e) {
    std::cerr << "Save error: " << e.what() << std::endl;
  }
}

std::string ImageReceiver::GetFrameTypeString(OBFrameType type) {
  switch (type) {
    case OB_FRAME_COLOR:
      return "Color";
    case OB_FRAME_DEPTH:
      return "Depth";
    case OB_FRAME_IR:
      return "IR";
    case OB_FRAME_IR_LEFT:
      return "IR_Left";
    case OB_FRAME_IR_RIGHT:
      return "IR_Right";
    case OB_FRAME_GYRO:
      return "Gyro";
    case OB_FRAME_ACCEL:
      return "Accel";
    default:
      return "Unknown";
  }
}

std::string ImageReceiver::GetTimestampString() {
  auto now = std::chrono::system_clock::now();
  auto now_time_t = std::chrono::system_clock::to_time_t(now);
  auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now.time_since_epoch()) %
                1000;

  std::stringstream ss;
  ss << std::put_time(std::localtime(&now_time_t), "%Y%m%d_%H%M%S") << "_"
     << std::setfill('0') << std::setw(3) << now_ms.count();
  return ss.str();
}

bool ImageReceiver::IsVideoFrame(OBFrameType type) {
  return type == OB_FRAME_COLOR || type == OB_FRAME_DEPTH ||
         type == OB_FRAME_IR || type == OB_FRAME_IR_LEFT ||
         type == OB_FRAME_IR_RIGHT;
}

bool ImageReceiver::IsIMUFrame(OBFrameType type) {
  return type == OB_FRAME_GYRO || type == OB_FRAME_ACCEL;
}

void ImageReceiver::HandleError(ob::Error &e) {
  std::cerr << "Error:\n"
            << "function:" << e.getName() << "\n"
            << "args:" << e.getArgs() << "\n"
            << "message:" << e.getMessage() << "\n"
            << "type:" << e.getExceptionType() << std::endl;
}

void ImageReceiver::Cleanup() {
  try {
    // 1. 停止pipeline
    std::cout << "Stopping pipeline..." << std::endl;
    pipe_.stop();

    // 2. 清理帧缓存
    {
      std::unique_lock<std::mutex> lock(frameMutex_);
      frameMap_.clear();
    }

    // 3. 关闭窗口
    std::cout << "Closing window..." << std::endl;
    app.close();

    std::cout << "Cleanup completed" << std::endl;
  } catch (const std::exception &e) {
    std::cerr << "Error in cleanup: " << e.what() << std::endl;
  }
}

ImageReceiver::~ImageReceiver() {
  Cleanup();  // 使用统一的清理函数
}

int main([[maybe_unused]] int argc, [[maybe_unused]] char **argv) {
  ImageReceiver receiver;
  receiver.Run();

  std::this_thread::sleep_for(std::chrono::seconds(2));
  return 0;
}