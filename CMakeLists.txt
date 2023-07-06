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
include_directories(${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/Remote_Client ${PROJECT_SOURCE_DIR}/Remote_Server)
include_directories(/usr/local/include /usr/include /usr/local/include/opencv4)

# add_executable(pullVideo ${remote_client} pull_stream_with_control_keyboard_and_mouse.cpp)
# add_executable(read_camera openCV_read_camera.cpp)
# add_executable(read_camera test_encode.cpp)
# add_executable(pullVideo ffmpeg_example.c)
# add_executable(remote_stream ${remote_server} push_stream_with_control_keyboard_and_mouse.cpp)
# add_executable(push_camera_rtmp ${remote_server} capture_screen.cpp push_stream_with_ffmpeg.cpp)

# remove cyclonedds update [2023/07/06]
add_executable(pullVideo pull_stream_with_control_keyboard_and_mouse.cpp)
add_executable(remote_stream  push_stream_with_control_keyboard_and_mouse.cpp)
add_executable(push_camera_rtmp capture_screen.cpp push_stream_with_ffmpeg.cpp)
# target_link_libraries(pullVideo 
# /usr/local/lib /usr/lib
# -lavformat -lavcodec -lavutil -lswscale -lz -lpthread -lswresample -llzma -lm -lSDL2 -lSDL2_ttf -lopencv4)

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

# Find the CycloneDDS package.

# file(GLOB_RECURSE self_cpps *.cpp)
# 推送视频
target_link_libraries(push_camera_rtmp 
-lavformat -lavcodec -lavutil -lswscale -lavdevice -lz -lpthread -lswresample -llzma -lm -lSDL2 -lSDL2_ttf -lX11 ${OpenCV_LIBS}) # DDSMsg_lib CycloneDDS::ddsc

target_link_libraries(remote_stream 
-lavformat -lavcodec -lavutil -lswscale -lavdevice -lz -lpthread -lswresample -llzma -lm -lSDL2 -lSDL2_ttf -lX11 ${OpenCV_LIBS}) # DDSMsg_lib CycloneDDS::ddsc
# 拉取视频流
target_link_libraries(pullVideo
-lavformat -lavcodec -lavutil -lswscale -lavdevice -lz -lpthread -lswresample -llzma -lm -lSDL2 -lSDL2_ttf ${OpenCV_LIBS}) #  DDSMsg_lib CycloneDDS::ddsc
