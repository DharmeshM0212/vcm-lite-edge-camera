#pragma once

#include "frame.hpp"

struct ImageStats {
    double mean_brightness;
    double contrast;
    double sharpness;
    double noise;
};

ImageStats compute_image_stats(const Frame& frame);