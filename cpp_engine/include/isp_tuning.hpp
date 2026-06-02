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
    std::string profile;
};

double choose_brightness_gain(double mean_brightness);
double choose_gamma(double mean_brightness);
IspSettings choose_isp_settings(const ImageStats& stats);
Frame apply_brightness_gain(const Frame& input, double gain);
Frame apply_gamma_correction(const Frame& input, double gamma);
Frame apply_isp_tuning(const Frame& input, const IspSettings& settings);