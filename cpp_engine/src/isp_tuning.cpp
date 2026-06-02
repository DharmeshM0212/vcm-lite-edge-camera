#include "isp_tuning.hpp"

#include "opencv_bridge.hpp"

#include <algorithm>
#include <cmath>
#include <opencv2/opencv.hpp>
#include <vector>

double choose_brightness_gain(double mean_brightness) {
    if (mean_brightness < 55.0) {
        return 1.75;
    }

    if (mean_brightness < 85.0) {
        return 1.45;
    }

    if (mean_brightness < 115.0) {
        return 1.20;
    }

    if (mean_brightness > 210.0) {
        return 0.80;
    }

    if (mean_brightness > 180.0) {
        return 0.90;
    }

    return 1.0;
}

double choose_gamma(double mean_brightness) {
    if (mean_brightness < 55.0) {
        return 0.65;
    }

    if (mean_brightness < 85.0) {
        return 0.78;
    }

    if (mean_brightness > 205.0) {
        return 1.30;
    }

    if (mean_brightness > 180.0) {
        return 1.15;
    }

    return 1.0;
}

IspSettings choose_isp_settings(const ImageStats& stats) {
    IspSettings settings;
    settings.brightness_gain = choose_brightness_gain(stats.mean_brightness);
    settings.gamma = choose_gamma(stats.mean_brightness);
    settings.contrast_alpha = 1.0;
    settings.contrast_beta = 0.0;
    settings.denoise_strength = 0.0;
    settings.sharpen_amount = 0.25;
    settings.clahe_enabled = false;
    settings.profile = "normal";

    if (stats.mean_brightness < 70.0) {
        settings.profile = "low_light";
        settings.contrast_alpha = 1.18;
        settings.contrast_beta = 4.0;
        settings.denoise_strength = stats.noise > 5.0 ? 0.30 : 0.18;
        settings.sharpen_amount = 0.35;
        settings.clahe_enabled = true;
        return settings;
    }

    if (stats.mean_brightness > 190.0) {
        settings.profile = "highlight_control";
        settings.contrast_alpha = 0.92;
        settings.contrast_beta = -6.0;
        settings.denoise_strength = 0.08;
        settings.sharpen_amount = 0.18;
        settings.clahe_enabled = false;
        return settings;
    }

    if (stats.noise > 7.0) {
        settings.profile = "high_noise";
        settings.contrast_alpha = 1.05;
        settings.contrast_beta = 0.0;
        settings.denoise_strength = 0.38;
        settings.sharpen_amount = 0.15;
        settings.clahe_enabled = false;
        return settings;
    }

    if (stats.contrast < 35.0) {
        settings.profile = "low_contrast";
        settings.contrast_alpha = 1.22;
        settings.contrast_beta = 2.0;
        settings.denoise_strength = 0.12;
        settings.sharpen_amount = 0.30;
        settings.clahe_enabled = true;
        return settings;
    }

    if (stats.sharpness < 80.0) {
        settings.profile = "soft_frame";
        settings.contrast_alpha = 1.08;
        settings.contrast_beta = 0.0;
        settings.denoise_strength = 0.05;
        settings.sharpen_amount = 0.45;
        settings.clahe_enabled = false;
        return settings;
    }

    return settings;
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

Frame apply_gamma_correction(const Frame& input, double gamma) {
    Frame output = input;
    double inv_gamma = 1.0 / gamma;

    std::vector<std::uint8_t> lut(256);

    for (int i = 0; i < 256; i++) {
        double normalized = static_cast<double>(i) / 255.0;
        double corrected = std::pow(normalized, inv_gamma) * 255.0;
        corrected = std::clamp(corrected, 0.0, 255.0);
        lut[static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(std::round(corrected));
    }

    for (auto& value : output.data) {
        value = lut[static_cast<std::size_t>(value)];
    }

    return output;
}

Frame apply_isp_tuning(const Frame& input, const IspSettings& settings) {
    cv::Mat mat = frame_to_mat(input);
    cv::Mat working;

    mat.convertTo(working, -1, settings.brightness_gain * settings.contrast_alpha, settings.contrast_beta);

    if (settings.gamma != 1.0) {
        std::vector<std::uint8_t> lut(256);
        double inv_gamma = 1.0 / settings.gamma;

        for (int i = 0; i < 256; i++) {
            double normalized = static_cast<double>(i) / 255.0;
            double corrected = std::pow(normalized, inv_gamma) * 255.0;
            corrected = std::clamp(corrected, 0.0, 255.0);
            lut[static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(std::round(corrected));
        }

        cv::Mat table(1, 256, CV_8U, lut.data());
        cv::LUT(working, table, working);
    }

    if (settings.clahe_enabled && working.channels() == 3) {
        cv::Mat lab;
        cv::cvtColor(working, lab, cv::COLOR_BGR2Lab);

        std::vector<cv::Mat> channels;
        cv::split(lab, channels);

        cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(2.0, cv::Size(8, 8));
        clahe->apply(channels[0], channels[0]);

        cv::merge(channels, lab);
        cv::cvtColor(lab, working, cv::COLOR_Lab2BGR);
    }

    if (settings.denoise_strength > 0.0) {
        cv::Mat blurred;
        cv::GaussianBlur(working, blurred, cv::Size(3, 3), 0.0);
        cv::addWeighted(working, 1.0 - settings.denoise_strength, blurred, settings.denoise_strength, 0.0, working);
    }

    if (settings.sharpen_amount > 0.0) {
        cv::Mat blurred;
        cv::GaussianBlur(working, blurred, cv::Size(0, 0), 1.0);
        cv::addWeighted(working, 1.0 + settings.sharpen_amount, blurred, -settings.sharpen_amount, 0.0, working);
    }

    return mat_to_frame(working, input.id);
}