# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

# 创建core库
set(CORE_SOURCES
    ImageReceiver.cpp
    DeviceManager.cpp
    MetadataHelper.cpp
    DumpHelper.cpp
    PerceptionSystem.cpp
)

set(CORE_HEADERS
    ImageReceiver.hpp
    DeviceManager.hpp
    MetadataHelper.hpp
    DumpHelper.hpp
    PerceptionSystem.hpp
    ConfigHelper.hpp
)

# 创建静态库
add_library(perception_core STATIC
    ${CORE_SOURCES}
    ${CORE_HEADERS}
)

# 链接依赖库
target_link_libraries(perception_core PUBLIC 
    ob::OrbbecSDK
    ob::examples::utils
    pthread
)

# 提供别名
add_library(perception::core ALIAS perception_core)

# 设置输出属性
set_target_properties(perception_core PROPERTIES
    FOLDER "perception"
    POSITION_INDEPENDENT_CODE ON
) 