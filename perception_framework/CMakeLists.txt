# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.10)
project(perception_app)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找必要的包
set(OpenCV_DIR "/home/seb/Workspace/project_camera/OrbbecSDK_v2/3rdparty/opencv/lib/cmake/opencv4")
find_package(OpenCV QUIET)

# 设置CMake模块路径
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})

# 设置所有可执行文件的输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# 设置通用编译选项
if(MSVC)
    add_compile_options(/W3)
else()
    add_compile_options(-Wall -Wextra)
endif()

# 统一设置头文件包含路径
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/config
    ${CMAKE_CURRENT_SOURCE_DIR}/com
    ${CMAKE_CURRENT_SOURCE_DIR}/com_qs
    ${CMAKE_CURRENT_SOURCE_DIR}/inference
    ${CMAKE_CURRENT_SOURCE_DIR}/calibration
    ${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty/jsoncpp/src/include/
)

# 添加子目录 - 先添加工具库
add_subdirectory(utils)
add_subdirectory(config)
add_subdirectory(core)
add_subdirectory(com)
add_subdirectory(inference)
add_subdirectory(calibration)

#----------------------------------------------------------------------
# perception_app应用
#----------------------------------------------------------------------
add_executable(perception_app main.cpp)

# 链接库 - 使用统一的perception命名
target_link_libraries(perception_app PRIVATE
    perception::core
    perception::config
    perception::com
    perception::utils
    perception::inference
    perception::calibration
    ob::OrbbecSDK
)

# 安装
install(TARGETS perception_app RUNTIME DESTINATION bin)

#----------------------------------------------------------------------
# state_tester应用
#----------------------------------------------------------------------
add_executable(state_tester StateControlTester.cpp)

# 链接库
target_link_libraries(state_tester PRIVATE
    perception::config
    perception::com
    perception::utils
    ob::OrbbecSDK
)

# 安装
install(TARGETS state_tester RUNTIME DESTINATION bin)

# 添加测试目标
add_custom_target(run_state_test
    COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/state_tester
    DEPENDS state_tester
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    COMMENT "Running state tester..."
)

add_custom_target(run_perception_app
    COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/perception_app
    DEPENDS perception_app
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    COMMENT "Running perception_app application..."
)

