cmake_minimum_required(VERSION 3.16)

project(vcm_lite_engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenCV REQUIRED)

add_executable(vcm_lite_engine
    src/main.cpp
    src/ai_detector.cpp
    src/metrics.cpp
    src/config.cpp
    src/frame.cpp
    src/frame_queue.cpp
    src/image_statistics.cpp
    src/isp_tuning.cpp
    src/roi.cpp
    src/motion_roi.cpp
    src/roi_packer.cpp
    src/semantic_encoder.cpp
    src/semantic_reconstruct.cpp
    src/rate_controller.cpp
    src/jpeg_encoder.cpp
    src/stability.cpp
    src/metadata.cpp
    src/log_writer.cpp
    src/opencv_bridge.cpp
    src/video_source.cpp
    src/output_writer.cpp
    src/detection_event_writer.cpp
)

target_include_directories(vcm_lite_engine PRIVATE include ${OpenCV_INCLUDE_DIRS})
target_link_libraries(vcm_lite_engine PRIVATE ${OpenCV_LIBS})