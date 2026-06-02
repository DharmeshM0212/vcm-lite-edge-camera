#pragma once

#include "frame.hpp"

#include <cstdint>
#include <opencv2/opencv.hpp>

Frame mat_to_frame(const cv::Mat& mat, std::uint64_t id);
cv::Mat frame_to_mat(const Frame& frame);