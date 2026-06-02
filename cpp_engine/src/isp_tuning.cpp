#include "isp_tuning.hpp"

#include <algorithm>
#include <cmath>

Frame apply_brightness_gain(const Frame& input, double gain) {
    Frame output = input;

    for (auto& value : output.data) {
        double scaled = static_cast<double>(value) * gain;
        scaled = std::clamp(scaled, 0.0, 255.0);
        value = static_cast<std::uint8_t>(std::round(scaled));
    }

    return output;
}