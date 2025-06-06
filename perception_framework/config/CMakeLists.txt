# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.10)

# 查找 jsoncpp 包
find_package(PkgConfig QUIET)
pkg_check_modules(JSONCPP jsoncpp)

# 创建config库
set(CONFIG_SOURCES
    ConfigHelper.cpp
    ConfigParser.cpp
)

set(CONFIG_HEADERS
    ConfigHelper.hpp
    ConfigParser.hpp
)

# 创建静态库
add_library(perception_config STATIC
    ${CONFIG_SOURCES}
    ${CONFIG_HEADERS}
)

# 设置include目录，包含jsoncpp路径
target_include_directories(perception_config PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../3rdparty/jsoncpp/src/include
)

# 链接依赖库
target_link_libraries(perception_config PUBLIC 
    perception::utils
    jsoncpp_static
    ${JSONCPP_LIBRARIES}
)

# 添加编译器特定的包含路径
if(JSONCPP_FOUND)
    target_include_directories(perception_config PRIVATE ${JSONCPP_INCLUDE_DIRS})
    target_compile_options(perception_config PRIVATE ${JSONCPP_CFLAGS_OTHER})
endif()

# 如果没有找到系统的jsoncpp，尝试使用header-only版本
if(NOT JSONCPP_FOUND)
    message(STATUS "System jsoncpp not found, you may need to install libjsoncpp-dev")
    # 可以在这里添加备用的json库，比如nlohmann/json
endif()

# 提供别名
add_library(perception::config ALIAS perception_config)

# 设置属性
set_target_properties(perception_config PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    
    POSITION_INDEPENDENT_CODE ON
    FOLDER "perception"
)

# 安装
install(TARGETS perception_config
    EXPORT perception_configTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

install(FILES
    ConfigHelper.hpp
    ConfigParser.hpp
    DESTINATION include/perception_framework/config
) 