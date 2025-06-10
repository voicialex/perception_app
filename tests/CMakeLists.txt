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

# 设置通用头文件包含路径
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../perception_framework
    ${CMAKE_CURRENT_SOURCE_DIR}/../perception_framework/core
    ${CMAKE_CURRENT_SOURCE_DIR}/../perception_framework/inference
    ${CMAKE_CURRENT_SOURCE_DIR}/../perception_framework/calibration
    ${CMAKE_CURRENT_SOURCE_DIR}/../perception_framework/utils
    ${CMAKE_CURRENT_SOURCE_DIR}/../perception_framework/com
    ${CMAKE_CURRENT_SOURCE_DIR}/../perception_framework/config
)

#----------------------------------------------------------------------
# test_nosignal_optimization - 无信号画面优化测试
#----------------------------------------------------------------------

if(OpenCV_FOUND)
    add_executable(test_nosignal_optimization test_nosignal_optimization.cpp)

    # 链接库
    target_link_libraries(test_nosignal_optimization PRIVATE
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
    target_include_directories(test_nosignal_optimization PRIVATE ${OpenCV_INCLUDE_DIRS})

    # 安装
    install(TARGETS test_nosignal_optimization RUNTIME DESTINATION bin)
endif()

#----------------------------------------------------------------------
# StateControlTester - 状态控制测试器
#----------------------------------------------------------------------
add_executable(state_tester StateControlTester.cpp)

# 链接库
target_link_libraries(state_tester PRIVATE
    perception::core
    perception::config
    perception::com
    perception::utils
    perception::inference
    perception::calibration
    ob::OrbbecSDK
)

# 安装
install(TARGETS state_tester RUNTIME DESTINATION bin)

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
    perception::inference
    perception::calibration
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
    perception::core
    perception::config
    perception::com
    perception::utils
    perception::inference
    perception::calibration
    ob::OrbbecSDK
)

# 安装
install(TARGETS config_usage_example RUNTIME DESTINATION bin)

# 添加测试目标
add_custom_target(run_nosignal_test
    COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_nosignal_optimization
    DEPENDS test_nosignal_optimization
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    COMMENT "Running no signal optimization test..."
)

add_custom_target(run_state_tester
    COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/state_tester
    DEPENDS state_tester
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    COMMENT "Running state control tester..."
)

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

# 添加运行所有测试的目标
add_custom_target(run_all_tests
    DEPENDS test_nosignal_optimization state_tester camera_bin inference_demo config_usage_example
    COMMENT "Building all test programs..."
) 