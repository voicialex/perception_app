cmake_minimum_required(VERSION 3.14...3.27)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# project
project(
  com
  VERSION 0.1.0
  DESCRIPTION "communication for uart"
  LANGUAGES C CXX
)

# common
include(GNUInstallDirs)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/lib)

option(ENABLE_TEST "Enable Test" ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif()

set(ENV{PKG_CONFIG_PATH} /opt/FriendlyARM/toolchain/6.4-aarch64/aarch64-cortexa53-linux-gnu/sysroot/usr/lib/pkgconfig)
set(PKG_CONFIG_SYSROOT_DIR /opt/FriendlyARM/toolchain/6.4-aarch64/aarch64-cortexa53-linux-gnu/sysroot)

find_package(PkgConfig)
pkg_check_modules(gtk  REQUIRED gtk+-3.0)


set(GTK3_FOUND ${GTK3_FOUND})
set(GTK3_INCLUDE_DIRS ${GTK3_INCLUDE_DIRS})
set(GTK3_LIBRARIES ${GTK3_LIBRARIES})
set(GTK3_VERSION ${GTK3_VERSION})

message(STATUS "GTK3_INCLUDE_DIRS: ${GTK3_INCLUDE_DIRS}")
message(STATUS "GTK3_LIBRARIES: ${GTK3_LIBRARIES}")

message(---------${GTK3_FOUND} ${PKG_CONFIG_PATH} ${PKG_CONFIG_SYSROOT_DIR})

include_directories(${CMAKE_SOURCE_DIR}/../thirdparty/asio/asio/include)

# src
file(GLOB_RECURSE PROJECT_HDRS ${PROJECT_SOURCE_DIR}/inc/*.h)
file(GLOB_RECURSE PROJECT_SRCS ${PROJECT_SOURCE_DIR}/src/*.cc)

add_library(${PROJECT_NAME} STATIC ${PROJECT_HDRS} ${PROJECT_SRCS})

target_include_directories(
  ${PROJECT_NAME}
  PUBLIC ${CMAKE_SOURCE_DIR}/inc ${CMAKE_SOURCE_DIR}/src
  INTERFACE $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/inc>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}>
)

if(ENABLE_TEST)
  # src
  file(GLOB_RECURSE MAIN_SRCS ${PROJECT_SOURCE_DIR}/test/light_main.cc)
  add_executable(light_service ${MAIN_SRCS})
  target_include_directories(light_service PRIVATE ${CMAKE_SOURCE_DIR}/inc)
  target_link_libraries(light_service PRIVATE com)

  # src
  file(GLOB_RECURSE MAIN_SRCS ${PROJECT_SOURCE_DIR}/test/session_main.cc)
  add_executable(session_service ${MAIN_SRCS})
  target_include_directories(session_service PRIVATE ${CMAKE_SOURCE_DIR}/inc)
  target_link_libraries(session_service PRIVATE com)
endif()

set(CMAKE_INSTALL_PREFIX $ENV{RUNTIME_INSTALL_PATH})

# install(FILES ${PROJECT_HDRS} DESTINATION $ENV{RUNTIME_INSTALL_PATH}/${PROJECT_NAME})
# install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION $ENV{RUNTIME_INSTALL_PATH})

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})