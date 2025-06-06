cmake_minimum_required(VERSION 3.5)

project(perception_app VERSION 2.4.3 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# 设置 OrbbecSDK 为主项目模式，以确保正确配置输出目录
set(OB_BUILD_MAIN_PROJECT TRUE)

add_subdirectory(3rdparty/OrbbecSDK_v2)
add_subdirectory(perception_app)

