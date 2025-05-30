# 相机标定模块
project(calibration_module)

# 查找OpenCV
find_package(OpenCV REQUIRED)
if(OpenCV_FOUND)
    message(STATUS "Found OpenCV version: ${OpenCV_VERSION}")
else()
    message(FATAL_ERROR "OpenCV not found!")
endif()

# 包含目录
include_directories(${OpenCV_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../core)

# 获取Orbbec SDK包含路径
get_target_property(ORBBEC_INCLUDE_DIRS ob::OrbbecSDK INTERFACE_INCLUDE_DIRECTORIES)
if(ORBBEC_INCLUDE_DIRS)
    include_directories(${ORBBEC_INCLUDE_DIRS})
else()
    # 备用路径
    include_directories(${CMAKE_SOURCE_DIR}/include)
endif()

# 标定模块源文件
set(CALIBRATION_SOURCES
    CalibrationManager.cpp
)

# 标定模块头文件
set(CALIBRATION_HEADERS
    CalibrationManager.hpp
)

# 创建标定模块库
add_library(calibration STATIC
    ${CALIBRATION_SOURCES}
    ${CALIBRATION_HEADERS}
)

# 添加别名以保持一致的命名
add_library(perception::calibration ALIAS calibration)

# 链接库
target_link_libraries(calibration
    ${OpenCV_LIBS}
    ob::OrbbecSDK
)

# 设置编译选项
target_compile_features(calibration PRIVATE cxx_std_14)

# 安装头文件
install(FILES ${CALIBRATION_HEADERS}
    DESTINATION include/calibration
)

# 安装库文件
install(TARGETS calibration
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

message(STATUS "Calibration module configured successfully with alias perception::calibration") 