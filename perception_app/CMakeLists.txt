# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.10)
project(demo)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找必要的包 - 不使用OpenCV
# set(OpenCV_DIR "/usr/lib/x86_64-linux-gnu/cmake/opencv4")
set(OpenCV_DIR "/home/seb/Workspace/project_camera/OrbbecSDK_v2/3rdparty/opencv/lib/cmake/opencv4")
# dependecies
find_package(OpenCV QUIET)

add_subdirectory(utils)

# 添加源文件
set(SOURCES
    main.cpp
    core/ImageReceiver.cpp
    core/DeviceManager.cpp
    core/MetadataHelper.cpp
    core/DumpHelper.cpp
    core/PerceptionSystem.cpp
    com/CommunicationProxy.cpp
    com/FifoComm.cpp
)

# 创建可执行文件
add_executable(${PROJECT_NAME} ${SOURCES})

# 包含头文件目录
target_include_directories(${PROJECT_NAME} PRIVATE
    # ${OpenCV_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/com
)

# 链接库
target_link_libraries(${PROJECT_NAME} PRIVATE
    pthread
    ob::OrbbecSDK
    ob::examples::utils
)

# 设置编译选项
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W3)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra)
endif()

# 设置输出目录
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "examples")

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)

# 添加测试程序
add_executable(state_tester 
    StateControlTester.cpp
    com/CommunicationProxy.cpp
    com/FifoComm.cpp
)

# 包含头文件目录
target_include_directories(state_tester PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/com
)

# 链接库
target_link_libraries(state_tester PRIVATE
    pthread
    ob::OrbbecSDK
    ob::examples::utils
)

# 设置输出目录
set_target_properties(state_tester PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

