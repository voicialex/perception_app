# Setup toolchain
sudo ./build-env-on-ubuntu-bionic/install.sh


# How to build

mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=~/Workspace/project_camera/OrbbecSDK_v2/cmake/aarch64-11.3-toolchain.cmake -DCMAKE_INSTALL_PREFIX=/home/seb/Workspace/project_camera/OrbbecSDK_v2/3rdparty/opencv
//  -DCMAKE_VERBOSE_MAKEFILE=ON

make -j8

# Run
cd build/bin
terminal_1:     ./demo
terminal_2:     ./state_tester

