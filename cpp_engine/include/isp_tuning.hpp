#pragma once

#include "frame.hpp"

double choose_brightness_gain(double mean_brightness);
double choose_gamma(double mean_brightness);
Frame apply_brightness_gain(const Frame& input, double gain);
Frame apply_gamma_correction(const Frame& input, double gamma);