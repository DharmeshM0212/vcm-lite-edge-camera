#pragma once

#include "frame.hpp"
#include "jpeg_encoder.hpp"

struct StabilityResult {
    double brightness_loss;
    double contrast_loss;
    double sharpness_loss;
    double total_loss;
};

StabilityResult compute_roi_stability_loss(const Frame& reference_roi_tile, const EncodedImage& compressed_roi_tile);