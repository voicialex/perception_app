# OrbbecSDK 推理框架

## 概述

这是一个为 OrbbecSDK v2 设计的推理框架，集成了模型推理和相机标定功能。该框架与现有的 perception_app 架构完全兼容，提供了一套完整的计算机视觉解决方案。

## 功能特性

### 推理系统
- **多模型支持**: 支持分类、检测、分割模型
- **多引擎后端**: 当前支持 ONNX Runtime（可扩展其他引擎）
- **异步推理**: 支持异步推理队列，避免阻塞主线程
- **性能监控**: 内置推理时间统计和性能分析
- **结果可视化**: 自动可视化推理结果

### 相机标定系统
- **实时标定**: 支持程序运行时的实时相机标定
- **多种图像源**: 支持 Color、Depth、IR 等多种图像类型
- **进度回调**: 实时反馈标定进度
- **结果保存**: 自动保存标定参数和结果
- **畸变校正**: 支持基于标定结果的图像去畸变

## 架构设计

```
inference/
├── InferenceBase.hpp       # 推理基础接口
├── InferenceManager.hpp    # 推理管理器
├── InferenceManager.cpp
├── ONNXInference.hpp       # ONNX 推理引擎
├── ONNXInference.cpp
├── CameraCalibration.hpp   # 相机标定
├── CameraCalibration.cpp
└── CMakeLists.txt
```

## 集成方式

### 1. 在 ImageReceiver 中的集成

推理和标定功能已经集成到 `ImageReceiver` 类中：

```cpp
// 在 processFrame 方法中
if(inferenceEnabled_ && config.inferenceConfig.enableInference) {
    auto& inferenceManager = inference::InferenceManager::getInstance();
    if(inferenceManager.isInitialized()) {
        inferenceManager.processFrame(frame, frame->type());
    }
}

if(calibrationEnabled_ && config.calibrationConfig.enableCalibration && cameraCalibration_) {
    cameraCalibration_->processFrame(frame);
}
```

### 2. 配置系统集成

在 `ConfigHelper.hpp` 中添加了推理和标定配置：

```cpp
struct InferenceConfig {
    bool enableInference = false;
    std::string defaultModel = "";
    std::string defaultModelType = "";
    float defaultThreshold = 0.5f;
    // ... 更多配置项
} inferenceConfig;

struct CalibrationConfig {
    bool enableCalibration = false;
    int boardWidth = 9;
    int boardHeight = 6;
    float squareSize = 25.0f;
    // ... 更多配置项
} calibrationConfig;
```

## 使用方法

### 键盘控制

程序运行时支持以下键盘快捷键：

- `I` 或 `i`: 切换推理功能开关
- `C` 或 `c`: 开始/停止相机标定
- `S` 或 `s`: 显示推理性能统计
- `R` 或 `r`: 重置推理性能统计

### 推理模型加载

```cpp
// 创建推理配置
inference::InferenceConfig config;
config.enableInference = true;
config.defaultModel = "path/to/your/model.onnx";
config.defaultModelType = "detection";  // "classification", "detection", "segmentation"
config.defaultThreshold = 0.5f;

// 初始化推理管理器
auto& manager = inference::InferenceManager::getInstance();
manager.initialize(config);

// 加载额外模型
manager.loadModel("face_detection", "face_detector.onnx", "detection");
```

### 相机标定使用

```cpp
// 创建标定对象
auto calibration = std::make_unique<inference::CameraCalibration>();

// 设置标定配置
inference::CalibrationConfig config;
config.boardWidth = 9;          // 棋盘内角点宽度
config.boardHeight = 6;         // 棋盘内角点高度
config.squareSize = 25.0f;      // 每个方格的大小(mm)
config.minValidFrames = 10;     // 最少需要的有效帧数
config.maxFrames = 20;          // 最多采集的帧数

// 开始标定
calibration->startCalibration(config, progressCallback);
```

## 演示程序

提供了 `inference_demo` 程序来演示推理框架的功能：

```bash
cd build
make inference_demo
./bin/inference_demo
```

## 编译说明

推理框架已集成到主项目的构建系统中：

```bash
cd build
cmake ..
make perception_app  # 编译主程序
make inference_demo  # 编译演示程序
```

## 依赖库

- OpenCV 4.x (图像处理和相机标定)
- OrbbecSDK v2 (相机数据获取)
- C++14 或更高版本

## 扩展说明

### 添加新的推理引擎

1. 继承 `InferenceEngine` 基类
2. 实现 `initialize()` 和 `infer()` 方法
3. 在 `InferenceEngineFactory` 中注册新引擎类型

### 自定义推理结果类型

1. 继承 `InferenceResult` 基类
2. 实现必要的虚函数
3. 在对应的推理引擎中使用新的结果类型

## 性能优化建议

1. **异步推理**: 启用异步推理避免阻塞主线程
2. **推理间隔**: 设置合适的推理间隔，不必每帧都推理
3. **模型优化**: 使用优化过的模型格式
4. **图像预处理**: 在推理前进行必要的图像预处理

## 故障排除

### 常见问题

1. **推理初始化失败**: 检查模型文件路径和格式
2. **标定失败**: 确保棋盘图案清晰可见
3. **性能问题**: 检查推理间隔设置和异步模式

### 调试建议

1. 启用详细日志记录
2. 检查配置文件的正确性
3. 验证输入图像的格式和大小

## 更新日志

- v1.0: 初始版本，支持基础推理和标定功能
- 集成到 perception_app 架构
- 支持多种图像类型和推理模型 