#  minimum required cmake version: 3.1.15 support vs2019

cmake_minimum_required(VERSION 3.5)
project(WindowsMetaDataHelper)

add_executable(WindowsMetaDataHelper main.cpp metadata-helper.cpp)
set_property(TARGET WindowsMetaDataHelper PROPERTY CXX_STANDARD 11)
