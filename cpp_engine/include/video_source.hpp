#pragma once

#include "frame.hpp"

#include <cstdint>
#include <optional>
#include <opencv2/opencv.hpp>
#include <string>

class VideoSource {
public:
    explicit VideoSource(const std::string& path);

    bool is_opened() const;
    double fps() const;
    int width() const;
    int height() const;
    std::optional<Frame> read();

private:
    cv::VideoCapture capture_;
    std::uint64_t next_frame_id_;
};