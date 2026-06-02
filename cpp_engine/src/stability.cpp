#include "stability.hpp"

#include "image_statistics.hpp"

#include <algorithm>
#include <cmath>

StabilityResult compute_roi_stability_loss(const Frame& reference_roi_tile, const EncodedImage& compressed_roi_tile) {
    Frame decoded = decode_jpeg(compressed_roi_tile, reference_roi_tile.id);

    ImageStats reference_stats = compute_image_stats(reference_roi_tile);
    ImageStats decoded_stats = compute_image_stats(decoded);

    double brightness_den = std::max(1.0, reference_stats.mean_brightness);
    double contrast_den = std::max(1.0, reference_stats.contrast);
    double sharpness_den = std::max(1.0, reference_stats.sharpness);

    double brightness_loss = std::abs(reference_stats.mean_brightness - decoded_stats.mean_brightness) / brightness_den;
    double contrast_loss = std::abs(reference_stats.contrast - decoded_stats.contrast) / contrast_den;
    double sharpness_loss = std::abs(reference_stats.sharpness - decoded_stats.sharpness) / sharpness_den;

    brightness_loss = std::clamp(brightness_loss, 0.0, 1.0);
    contrast_loss = std::clamp(contrast_loss, 0.0, 1.0);
    sharpness_loss = std::clamp(sharpness_loss, 0.0, 1.0);

    StabilityResult result;
    result.brightness_loss = brightness_loss;
    result.contrast_loss = contrast_loss;
    result.sharpness_loss = sharpness_loss;
    result.total_loss = std::clamp(0.15 * brightness_loss + 0.25 * contrast_loss + 0.60 * sharpness_loss, 0.0, 1.0);
    return result;
}