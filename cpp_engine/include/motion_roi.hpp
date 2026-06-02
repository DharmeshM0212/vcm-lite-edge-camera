#pragma once

#include "frame.hpp"
#include "roi.hpp"

#include <optional>

class MotionRoiDetector {
public:
    MotionRoiDetector();

    RoiResult detect(const Frame& frame);

private:
    std::optional<Frame> previous_frame_;
};