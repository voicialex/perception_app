# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.10)
project(tests)

# 查找必要的包
set(OpenCV_DIR "/home/seb/Workspace/project_camera/OrbbecSDK_v2/3rdparty/opencv/lib/cmake/opencv4")
find_package(OpenCV QUIET)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置编译选项
if(MSVC)
    add_compile_options(/W3)
else()
    add_compile_options(-Wall -Wextra)
endif()

# 设置通用头文件包含路径 - 指向 perception_framework 目录
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../perception_framework
)

#----------------------------------------------------------------------
# camera_bin 测试应用
#----------------------------------------------------------------------
add_executable(camera_bin camera_bin.cpp)

# 链接库
target_link_libraries(camera_bin PRIVATE
    perception::core
    perception::config
    perception::com
    perception::utils
    ob::OrbbecSDK
)

# 安装
install(TARGETS camera_bin RUNTIME DESTINATION bin)

#----------------------------------------------------------------------
# inference_demo 测试应用
#----------------------------------------------------------------------
add_executable(inference_demo inference_demo.cpp)

# 链接库
target_link_libraries(inference_demo PRIVATE
    perception::core
    perception::config
    perception::com
    perception::utils
    perception::inference
    perception::calibration
    ob::OrbbecSDK
    ${OpenCV_LIBS}
)

# 添加OpenCV包含目录
if(OpenCV_FOUND)
    target_include_directories(inference_demo PRIVATE ${OpenCV_INCLUDE_DIRS})
endif()

# 安装
install(TARGETS inference_demo RUNTIME DESTINATION bin)

#----------------------------------------------------------------------
# config_usage_example 测试应用
#----------------------------------------------------------------------
add_executable(config_usage_example config_usage_example.cpp)

# 链接库
target_link_libraries(config_usage_example PRIVATE
    perception::config
    perception::core
)

# 安装
install(TARGETS config_usage_example RUNTIME DESTINATION bin)

# 添加测试目标
add_custom_target(run_camera_test
    COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/camera_bin
    DEPENDS camera_bin
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    COMMENT "Running camera test..."
)

add_custom_target(run_inference_demo
    COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/inference_demo
    DEPENDS inference_demo
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    COMMENT "Running inference demo..."
)

add_custom_target(run_config_example
    COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/config_usage_example
    DEPENDS config_usage_example
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    COMMENT "Running config usage example..."
) 