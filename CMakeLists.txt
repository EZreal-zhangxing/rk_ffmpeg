cmake_minimum_required(VERSION 3.10)

PROJECT(STREAM_REMOTE)
#设置目标处理器架构
# set(CMAKE_SYSTEM_PROCESSOR aarch64)
# 指定交叉编译器 arm-linux-gcc 和 arm-linux-g++
set(CMAKE_C_COMPILER  /usr/bin/gcc)
set(CMAKE_CXX_COMPILER /usr/bin/g++)

# set(CMAKE_C_FLAGS "-pipe -O2 -Wall -W")
# set(CMAKE_CXX_FLAGS "-pipe -O2 -std=gnu++11 -Wall -W -D_REENTRANT -fPIC")

message(STATUS "Project_source_dir: ${PROJECT_SOURCE_DIR}")

find_package(OpenCV REQUIRED)

# If the package has been found, several variables will
# be set, you can find the full list with descriptions
# in the OpenCVConfig.cmake file.
# Print some message showing some of them
message(STATUS "OpenCV library status:")
message(STATUS "    config: ${OpenCV_DIR}")
message(STATUS "    version: ${OpenCV_VERSION}")
message(STATUS "    libraries: ${OpenCV_LIBS}")
message(STATUS "    include path: ${OpenCV_INCLUDE_DIRS}")

set(SYS_INCLUDE /usr/local/include/)
set(SYS_LIB /usr/local/lib/)
include_directories(./include ${OpenCV_DIR} ${SYS_INCLUDE})
include_directories(${OpenCV_INCLUDE_DIRS})

# remove cyclonedds update [2023/07/06]
set(SYS_SRC ${PROJECT_SOURCE_DIR}/src)
add_executable(pullVideo ${SYS_SRC}/pull_stream_with_control_keyboard_and_mouse.cpp)
add_executable(remote_stream  ${SYS_SRC}/push_stream_with_control_keyboard_and_mouse.cpp)
add_executable(push_camera_rtmp ${SYS_SRC}/capture_screen.cpp ${SYS_SRC}/push_stream_with_ffmpeg.cpp)


# update[2023/7/26] add mpp encoder and decode
add_executable(rtsp_send_opencv_ffmpeg ${PROJECT_SOURCE_DIR}/src/rtsp_with_ffmpeg_using_opencv.cpp)

add_executable(rtsp_send_opencv_mpp ${PROJECT_SOURCE_DIR}/src/rtsp_with_mpp_using_opencv.cpp)

add_executable(rtsp_send_opencv_mpp_rgb ${PROJECT_SOURCE_DIR}/src/rtsp_with_mpp_using_opencv_rgb.cpp)

# file(GLOB_RECURSE self_cpps *.cpp)
# 推送视频
target_link_libraries(push_camera_rtmp 
-lavformat -lavcodec -lavutil -lswscale -lavdevice -lz -lpthread -lswresample -llzma -lm -lSDL2 -lX11 ${OpenCV_LIBS}) # DDSMsg_lib CycloneDDS::ddsc

target_link_libraries(remote_stream 
-lavformat -lavcodec -lavutil -lswscale -lavdevice -lz -lpthread -lswresample -llzma -lm -lSDL2 -lX11 ${OpenCV_LIBS}) # DDSMsg_lib CycloneDDS::ddsc
# 拉取视频流
target_link_libraries(pullVideo
-lavformat -lavcodec -lavutil -lswscale -lavdevice -lz -lpthread -lswresample -llzma -lm -lSDL2  ${OpenCV_LIBS}) #  DDSMsg_lib CycloneDDS::ddsc

target_link_libraries(rtsp_send_opencv_ffmpeg 
-lavformat -lavcodec -lavutil -lswscale -lavdevice -lz -lpthread -lswresample -llzma -lm ${OpenCV_LIBS} -lrockchip_mpp)

target_link_libraries(rtsp_send_opencv_mpp 
-lavformat -lavcodec -lavutil -lswscale -lavdevice -lz -lpthread -lswresample -llzma -lm ${OpenCV_LIBS} -lrockchip_mpp)

target_link_libraries(rtsp_send_opencv_mpp_rgb
-lavformat -lavcodec -lavutil -lswscale -lavdevice -lz -lpthread -lswresample -llzma -lm ${OpenCV_LIBS} -lrockchip_mpp)