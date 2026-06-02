#pragma once

#include "frame.hpp"

double choose_brightness_gain(double mean_brightness);
Frame apply_brightness_gain(const Frame& input, double gain);