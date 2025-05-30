# 推理框架模块
project(inference_framework)

# 查找OpenCV
find_package(OpenCV REQUIRED)
if(OpenCV_FOUND)
    message(STATUS "Found OpenCV version: ${OpenCV_VERSION}")
else()
    message(FATAL_ERROR "OpenCV not found!")
endif()

# 包含目录
include_directories(${OpenCV_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../core)

# 获取Orbbec SDK包含路径
get_target_property(ORBBEC_INCLUDE_DIRS ob::OrbbecSDK INTERFACE_INCLUDE_DIRECTORIES)
if(ORBBEC_INCLUDE_DIRS)
    include_directories(${ORBBEC_INCLUDE_DIRS})
else()
    # 备用路径
    include_directories(${CMAKE_SOURCE_DIR}/include)
endif()

# 推理框架源文件
set(INFERENCE_SOURCES
    ONNXInference.cpp
    InferenceManager.cpp
)

# 推理框架头文件
set(INFERENCE_HEADERS
    ONNXInference.hpp
    InferenceManager.hpp
    InferenceBase.hpp
)

# 创建推理框架库
add_library(ob_inference_framework STATIC
    ${INFERENCE_SOURCES}
    ${INFERENCE_HEADERS}
)

# 添加别名以保持一致的命名
add_library(perception::inference ALIAS ob_inference_framework)

# 链接库
target_link_libraries(ob_inference_framework
    ${OpenCV_LIBS}
    ob::OrbbecSDK
)

# 设置编译选项
target_compile_features(ob_inference_framework PRIVATE cxx_std_14)

# 安装头文件
install(FILES ${INFERENCE_HEADERS}
    DESTINATION include/inference
)

# 安装库文件
install(TARGETS ob_inference_framework
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

message(STATUS "Inference framework configured successfully with alias perception::inference") 