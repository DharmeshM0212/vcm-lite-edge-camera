cmake_minimum_required(VERSION 3.16)

project(vcm_lite_engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(vcm_lite_engine
    src/main.cpp
    src/metrics.cpp
    src/config.cpp
    src/frame.cpp
    src/frame_queue.cpp
    src/image_statistics.cpp
)

target_include_directories(vcm_lite_engine PRIVATE include)