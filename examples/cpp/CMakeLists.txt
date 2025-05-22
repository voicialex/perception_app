cmake_minimum_required(VERSION 3.5)

# 设置 OpenCV 路径
set(OpenCV_DIR "/usr/lib/x86_64-linux-gnu/cmake/opencv4")
find_package(OpenCV REQUIRED)
# 如果上面的路径不正确，可以尝试以下路径之一：
# set(OpenCV_DIR "/usr/local/lib/cmake/opencv4")
# set(OpenCV_DIR "/usr/share/opencv4")

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# add_subdirectory(Sample-DepthWorkMode)

# add_subdirectory(Sample-FirmwareUpgrade)
# add_subdirectory(Sample-HelloOrbbec)
# add_subdirectory(Sample-HotPlugin)
# add_subdirectory(Sample-ImuReader)
# add_subdirectory(Sample-SensorControl)
# add_subdirectory(Sample-Transformation)
# add_subdirectory(Sample-QuickStart)

# opencv required
if(${OpenCV_FOUND})
    add_subdirectory(Demo)
    message("Demo --------------------------")
    add_subdirectory(Sample-ColorViewer)
    # add_subdirectory(Sample-DepthViewer)
    # add_subdirectory(Sample-InfraredViewer)
    # add_subdirectory(Sample-DoubleInfraredViewer)
    # add_subdirectory(Sample-SyncAlignViewer)
	# add_subdirectory(Sample-AlignFilterViewer)
	# add_subdirectory(Sample-HdrMerge)
	# add_subdirectory(Sample-PostProcessing)
    # add_subdirectory(Sample-PointCloud)
    # add_subdirectory(Sample-MultiDevice)
    # # add_subdirectory(Sample-MultiDeviceSyncOld)
    # add_subdirectory(Sample-MultiDeviceSync)
    # add_subdirectory(Sample-MultiStream)
    # add_subdirectory(Sample-NetDevice)
    # add_subdirectory(Sample-SaveToDisk)
    # add_subdirectory(Sample-DepthUnitControl)
    # add_subdirectory(Sample-CommonUsages)
if(NOT APPLE)
    add_subdirectory(Sample-Recorder)
    add_subdirectory(Sample-Playback)
endif()
endif(${OpenCV_FOUND})

