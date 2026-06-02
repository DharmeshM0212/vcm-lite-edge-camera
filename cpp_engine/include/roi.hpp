#pragma once

#include "frame.hpp"

#include <vector>

struct RoiBox {
    int x;
    int y;
    int width;
    int height;
    double confidence;
};

struct RoiResult {
    std::vector<RoiBox> boxes;
};

RoiResult detect_synthetic_roi(const Frame& frame);