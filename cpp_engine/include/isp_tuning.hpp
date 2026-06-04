#pragma once

#include "frame.hpp"
#include "image_statistics.hpp"

#include <string>

struct IspSettings {
    double brightness_gain;
    double gamma;
    double contrast_alpha;
    double contrast_beta;
    double denoise_strength;
    double sharpen_amount;
    bool clahe_enabled;
    double red_gain;
    double green_gain;
    double blue_gain;
    double shadow_lift;
    double highlight_compression;
    double tone_strength;
    std::string profile;
};

IspSettings choose_isp_settings(const ImageStats& stats);
Frame apply_isp_tuning(const Frame& frame, const IspSettings& settings);