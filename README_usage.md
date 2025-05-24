# Setup toolchain
sudo ./build-env-on-ubuntu-bionic/install.sh


# How to build

mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/aarch64-toolchain.cmake -DCMAKE_VERBOSE_MAKEFILE=ON

make -j8

// code path: examples/0..Demo
