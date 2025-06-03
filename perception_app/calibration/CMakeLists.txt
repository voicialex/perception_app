# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.5)
project(calibration)

# 检查OpenCV是否可用
if(NOT OpenCV_FOUND)
    message(WARNING "OpenCV not found, skipping calibration module")
    return()
endif()

# 检查OrbbecSDK是否可用
get_target_property(OrbbecSDK_TARGET_TYPE ob::OrbbecSDK TYPE)
if(NOT OrbbecSDK_TARGET_TYPE)
    message(STATUS "OrbbecSDK target not found, calibration module will have limited functionality")
    set(HAS_ORBBEC_SDK FALSE)
else()
    set(HAS_ORBBEC_SDK TRUE)
endif()

# 创建标定库
add_library(calibration STATIC
    CalibrationManager.cpp
    CalibrationManager.hpp
)

# 设置包含目录
target_include_directories(calibration PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils
    ${CMAKE_CURRENT_SOURCE_DIR}/../core
)

# 链接基础依赖
target_link_libraries(calibration PUBLIC
    ${OpenCV_LIBS}
    perception::utils
    spdlog::spdlog
)

# 包含OpenCV头文件
target_include_directories(calibration PUBLIC ${OpenCV_INCLUDE_DIRS})

# 如果有OrbbecSDK，链接它
if(HAS_ORBBEC_SDK)
    target_link_libraries(calibration PUBLIC ob::OrbbecSDK)
    target_compile_definitions(calibration PUBLIC HAS_OBSENSOR)
endif()

# 定义标定功能可用的宏
target_compile_definitions(calibration PUBLIC HAS_CALIBRATION)

# 创建别名
add_library(perception::calibration ALIAS calibration)

# 设置目标属性
set_target_properties(calibration PROPERTIES FOLDER "perception")

# 安装头文件
install(FILES CalibrationManager.hpp
    DESTINATION include/calibration
)

# 安装库文件
install(TARGETS calibration
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

message(STATUS "Calibration module configured successfully with alias perception::calibration") 