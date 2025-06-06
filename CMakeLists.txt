cmake_minimum_required(VERSION 3.5)

# 设置CMake策略以避免警告
if(POLICY CMP0048)
    cmake_policy(SET CMP0048 NEW)  # project() command manages VERSION variables
endif()

if(POLICY CMP0063)
    cmake_policy(SET CMP0063 NEW)  # Honor visibility properties for all target types
endif()

if(POLICY CMP0064)
    cmake_policy(SET CMP0064 NEW)  # Support new TEST if() operator
endif()

if(POLICY CMP0072)
    cmake_policy(SET CMP0072 NEW)  # FindOpenGL prefers GLVND by default when available
endif()

if(POLICY CMP0077)
    cmake_policy(SET CMP0077 NEW)  # option() honors normal variables
endif()

# 设置OpenGL偏好以避免FindOpenGL警告
set(OpenGL_GL_PREFERENCE GLVND)

project(perception_app VERSION 2.4.3 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# 设置 OrbbecSDK 为主项目模式，以确保正确配置输出目录
set(OB_BUILD_MAIN_PROJECT TRUE)

add_subdirectory(3rdparty/OrbbecSDK_v2)
add_subdirectory(perception_framework)
add_subdirectory(tests)

