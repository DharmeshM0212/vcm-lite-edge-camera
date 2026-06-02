#include "isp_tuning.hpp"

#include <algorithm>
#include <cmath>

double choose_brightness_gain(double mean_brightness) {
    if (mean_brightness < 70.0) {
        return 1.6;
    }

    if (mean_brightness < 130.0) {
        return 1.25;
    }

    if (mean_brightness > 190.0) {
        return 0.85;
    }

    return 1.0;
}

Frame apply_brightness_gain(const Frame& input, double gain) {
    Frame output = input;

    for (auto& value : output.data) {
        double scaled = static_cast<double>(value) * gain;
        scaled = std::clamp(scaled, 0.0, 255.0);
        value = static_cast<std::uint8_t>(std::round(scaled));
    }

    return output;
}