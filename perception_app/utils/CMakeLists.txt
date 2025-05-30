# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.5)
project(ob_perception_utils)

# 添加头文件列表
set(HEADERS
    ${CMAKE_CURRENT_LIST_DIR}/utils_c.h
    ${CMAKE_CURRENT_LIST_DIR}/utils.hpp
    ${CMAKE_CURRENT_LIST_DIR}/utils_types.h
    ${CMAKE_CURRENT_LIST_DIR}/Logger.hpp
    ${CMAKE_CURRENT_LIST_DIR}/ThreadPool.hpp
)

add_library(ob_perception_utils STATIC
    ${CMAKE_CURRENT_LIST_DIR}/utils_c.c
    ${CMAKE_CURRENT_LIST_DIR}/utils.cpp
    ${HEADERS}
)

if(${OpenCV_FOUND})
    target_sources(ob_perception_utils PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/utils_opencv.cpp
        ${CMAKE_CURRENT_LIST_DIR}/utils_opencv.hpp
    )
    target_link_libraries(ob_perception_utils PUBLIC ${OpenCV_LIBS} ob::OrbbecSDK)
    target_include_directories(ob_perception_utils PUBLIC ${OpenCV_INCLUDE_DIRS})
endif()

find_package(Threads REQUIRED)
target_link_libraries(ob_perception_utils PUBLIC Threads::Threads)

target_include_directories(ob_perception_utils PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_library(perception::utils ALIAS ob_perception_utils)
set_target_properties(ob_perception_utils PROPERTIES FOLDER "perception")
