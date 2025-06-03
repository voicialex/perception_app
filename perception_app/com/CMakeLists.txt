# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

# 创建com库
set(COM_SOURCES
    CommunicationProxy.cpp
    FifoComm.cpp
)

set(COM_HEADERS
    CommunicationProxy.hpp
    FifoComm.hpp
    ICommunicationImpl.hpp
)

# 创建静态库
add_library(perception_com STATIC
    ${COM_SOURCES}
    ${COM_HEADERS}
)

# 链接依赖库
target_link_libraries(perception_com PUBLIC 
    ob::examples::utils
    perception::utils
    spdlog::spdlog
    pthread
)

# 提供别名
add_library(perception::com ALIAS perception_com)

# 设置输出属性
set_target_properties(perception_com PROPERTIES
    FOLDER "perception"
    POSITION_INDEPENDENT_CODE ON
) 