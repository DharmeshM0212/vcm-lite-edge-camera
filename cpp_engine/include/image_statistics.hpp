#pragma once

#include "frame.hpp"

struct ImageStats {
    double mean_brightness;
};

ImageStats compute_image_stats(const Frame& frame);