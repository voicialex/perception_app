# 设置CMake最低版本要求
cmake_minimum_required(VERSION 3.5)

# 设置CMake策略
cmake_policy(SET CMP0048 NEW)
cmake_policy(SET CMP0064 NEW)
cmake_policy(SET CMP0072 NEW)
cmake_policy(SET CMP0063 NEW)

# 设置目标系统类型
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(TOOLCHAIN_ARM64_A53 ON)

# 设置工具链路径
set(TOOLCHAIN_PATH /opt/FriendlyARM/toolchain/6.4-aarch64)

# 设置编译器
set(CMAKE_C_COMPILER ${TOOLCHAIN_PATH}/bin/aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PATH}/bin/aarch64-linux-gnu-g++)

# 设置查找根路径
set(CMAKE_FIND_ROOT_PATH ${TOOLCHAIN_PATH}/aarch64-cortexa53-linux-gnu/sysroot)

# 设置查找规则,设置编译器搜索路径
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# 设置编译器标志
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --sysroot=${CMAKE_FIND_ROOT_PATH}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --sysroot=${CMAKE_FIND_ROOT_PATH}")

# 设置链接器标志
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --sysroot=${CMAKE_FIND_ROOT_PATH}")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} --sysroot=${CMAKE_FIND_ROOT_PATH}")

# 设置线程库
set(CMAKE_THREAD_LIBS_INIT "-lpthread")
set(CMAKE_HAVE_THREADS_LIBRARY 1)
set(CMAKE_USE_WIN32_THREADS_INIT 0)
set(CMAKE_USE_PTHREADS_INIT 1)
set(Threads_FOUND TRUE)

# 强制使用交叉编译
set(CMAKE_CROSSCOMPILING TRUE)
set(CMAKE_CROSSCOMPILING_EMULATOR "")

# 设置OpenGL库路径
set(OPENGL_INCLUDE_DIR ${CMAKE_FIND_ROOT_PATH}/usr/include)
set(OPENGL_gl_LIBRARY ${CMAKE_FIND_ROOT_PATH}/usr/lib/aarch64-linux-gnu/libGL.so)
set(OPENGL_glx_LIBRARY ${CMAKE_FIND_ROOT_PATH}/usr/lib/aarch64-linux-gnu/libGLX.so)
