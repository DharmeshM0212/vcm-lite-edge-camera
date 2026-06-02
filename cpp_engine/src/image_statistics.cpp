#include "image_statistics.hpp"

ImageStats compute_image_stats(const Frame& frame) {
    double sum = 0.0;
    std::size_t pixel_count = static_cast<std::size_t>(frame.width * frame.height);

    for (std::size_t i = 0; i < pixel_count; i++) {
        std::size_t base = i * static_cast<std::size_t>(frame.channels);
        double r = frame.data[base];

        if (frame.channels == 1) {
            sum += r;
        } else {
            double g = frame.data[base + 1];
            double b = frame.data[base + 2];
            sum += 0.299 * r + 0.587 * g + 0.114 * b;
        }
    }

    ImageStats stats;
    stats.mean_brightness = sum / static_cast<double>(pixel_count);
    return stats;
}