# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.10)
project(demo)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)  # 升级到C++17以支持新特性
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找必要的包
find_package(OpenCV REQUIRED)
# find_package(OrbbecSDK REQUIRED)

# 添加源文件
set(SOURCES
    main.cpp
    ImageReceiver.cpp
    DeviceManager.cpp
    MetadataHelper.cpp
    DumpHelper.cpp
)

# 添加头文件
set(HEADERS
    ImageReceiver.hpp
    DeviceManager.hpp
    MetadataHelper.hpp
    DumpHelper.hpp
    ConfigHelper.hpp
    Logger.hpp
)

# 创建可执行文件
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# 包含头文件目录
target_include_directories(${PROJECT_NAME} PRIVATE
    ${OpenCV_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# 链接库
target_link_libraries(${PROJECT_NAME} PRIVATE ob::OrbbecSDK ob::examples::utils)

# 设置编译选项
target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

# 设置输出目录
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "examples")

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)

