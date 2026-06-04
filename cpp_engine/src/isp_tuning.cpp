#include "isp_tuning.hpp"

#include "opencv_bridge.hpp"

#include <algorithm>
#include <cmath>
#include <opencv2/opencv.hpp>

static double clamp_double(double value, double low, double high) {
    return std::max(low, std::min(high, value));
}

static double smooth_value(double previous, double proposed, double alpha) {
    return (1.0 - alpha) * previous + alpha * proposed;
}

static cv::Mat apply_white_balance(const cv::Mat& input, const IspSettings& settings) {
    cv::Mat float_image;
    input.convertTo(float_image, CV_32FC3);

    std::vector<cv::Mat> channels;
    cv::split(float_image, channels);

    channels[0] *= settings.blue_gain;
    channels[1] *= settings.green_gain;
    channels[2] *= settings.red_gain;

    cv::merge(channels, float_image);

    cv::Mat output;
    cv::threshold(float_image, float_image, 255.0, 255.0, cv::THRESH_TRUNC);
    cv::threshold(float_image, float_image, 0.0, 0.0, cv::THRESH_TOZERO);
    float_image.convertTo(output, CV_8UC3);

    return output;
}

static cv::Mat apply_gamma_curve(const cv::Mat& input, double gamma) {
    cv::Mat lut(1, 256, CV_8UC1);

    double safe_gamma = clamp_double(gamma, 0.35, 2.5);

    for (int i = 0; i < 256; i++) {
        double normalized = static_cast<double>(i) / 255.0;
        double corrected = std::pow(normalized, safe_gamma);
        lut.at<unsigned char>(0, i) = static_cast<unsigned char>(clamp_double(corrected * 255.0, 0.0, 255.0));
    }

    cv::Mat output;
    cv::LUT(input, lut, output);
    return output;
}

static cv::Mat apply_tone_curve(const cv::Mat& input, const IspSettings& settings) {
    cv::Mat lut(1, 256, CV_8UC1);

    double shadow_lift = clamp_double(settings.shadow_lift, 0.0, 0.35);
    double highlight_compression = clamp_double(settings.highlight_compression, 0.0, 0.45);
    double strength = clamp_double(settings.tone_strength, 0.0, 1.0);

    for (int i = 0; i < 256; i++) {
        double x = static_cast<double>(i) / 255.0;

        double shadow = x + shadow_lift * (1.0 - x) * (1.0 - x);
        double highlight = shadow - highlight_compression * shadow * shadow * std::max(0.0, shadow - 0.55);
        double s_curve = 1.0 / (1.0 + std::exp(-6.0 * (highlight - 0.5)));
        double normalized_s = (s_curve - 1.0 / (1.0 + std::exp(3.0))) / ((1.0 / (1.0 + std::exp(-3.0))) - (1.0 / (1.0 + std::exp(3.0))));

        double y = (1.0 - strength) * x + strength * normalized_s;
        lut.at<unsigned char>(0, i) = static_cast<unsigned char>(clamp_double(y * 255.0, 0.0, 255.0));
    }

    cv::Mat output;
    cv::LUT(input, lut, output);
    return output;
}

static cv::Mat apply_denoise(const cv::Mat& input, double strength) {
    double s = clamp_double(strength, 0.0, 1.0);

    if (s <= 0.02) {
        return input.clone();
    }

    cv::Mat filtered;

    if (s < 0.35) {
        cv::GaussianBlur(input, filtered, cv::Size(3, 3), 0.6 + 0.8 * s);
    } else {
        cv::fastNlMeansDenoisingColored(input, filtered, 3.0 + 8.0 * s, 3.0 + 8.0 * s, 7, 21);
    }

    cv::Mat output;
    cv::addWeighted(input, 1.0 - 0.45 * s, filtered, 0.45 * s, 0.0, output);
    return output;
}

static cv::Mat apply_sharpen(const cv::Mat& input, double amount) {
    double a = clamp_double(amount, 0.0, 1.0);

    if (a <= 0.02) {
        return input.clone();
    }

    cv::Mat blurred;
    cv::GaussianBlur(input, blurred, cv::Size(0, 0), 1.0);

    cv::Mat output;
    cv::addWeighted(input, 1.0 + a, blurred, -a, 0.0, output);
    return output;
}

static cv::Scalar channel_means(const cv::Mat& image) {
    return cv::mean(image);
}

IspSettings choose_isp_settings(const ImageStats& stats) {
    static bool initialized = false;
    static double smoothed_gain = 1.0;
    static double smoothed_gamma = 1.0;
    static double smoothed_denoise = 0.0;
    static double smoothed_sharpen = 0.1;

    IspSettings settings;
    settings.brightness_gain = 1.0;
    settings.gamma = 1.0;
    settings.contrast_alpha = 1.0;
    settings.contrast_beta = 0.0;
    settings.denoise_strength = 0.0;
    settings.sharpen_amount = 0.1;
    settings.clahe_enabled = false;
    settings.red_gain = 1.0;
    settings.green_gain = 1.0;
    settings.blue_gain = 1.0;
    settings.shadow_lift = 0.0;
    settings.highlight_compression = 0.0;
    settings.tone_strength = 0.25;
    settings.profile = "normal";

    double brightness = stats.mean_brightness;
    double contrast = stats.contrast;
    double noise = stats.noise;
    double sharpness = stats.sharpness;

    double target_luma = 105.0;
    double proposed_gain = target_luma / std::max(20.0, brightness);
    proposed_gain = clamp_double(proposed_gain, 0.70, 1.55);

    double proposed_gamma = 1.0;
    double proposed_denoise = 0.0;
    double proposed_sharpen = 0.12;

    if (brightness < 55.0) {
        settings.profile = "low_light";
        proposed_gain = clamp_double(proposed_gain * 1.10, 1.10, 1.65);
        proposed_gamma = 0.72;
        proposed_denoise = 0.45;
        proposed_sharpen = 0.12;
        settings.shadow_lift = 0.20;
        settings.highlight_compression = 0.12;
        settings.tone_strength = 0.45;
        settings.clahe_enabled = true;
    } else if (brightness > 165.0) {
        settings.profile = "bright_scene";
        proposed_gain = clamp_double(proposed_gain * 0.90, 0.60, 1.00);
        proposed_gamma = 1.18;
        proposed_denoise = 0.05;
        proposed_sharpen = 0.08;
        settings.shadow_lift = 0.02;
        settings.highlight_compression = 0.32;
        settings.tone_strength = 0.42;
    } else if (noise > 8.0) {
        settings.profile = "high_noise";
        proposed_gamma = 0.82;
        proposed_denoise = 0.38;
        proposed_sharpen = 0.14;
        settings.shadow_lift = 0.08;
        settings.highlight_compression = 0.15;
        settings.tone_strength = 0.34;
    } else if (contrast < 42.0) {
        settings.profile = "low_contrast";
        proposed_gamma = 0.90;
        proposed_denoise = 0.12;
        proposed_sharpen = 0.18;
        settings.shadow_lift = 0.07;
        settings.highlight_compression = 0.10;
        settings.tone_strength = 0.48;
        settings.clahe_enabled = true;
    } else if (sharpness < 700.0) {
        settings.profile = "soft_frame";
        proposed_gamma = 0.95;
        proposed_denoise = 0.08;
        proposed_sharpen = 0.28;
        settings.shadow_lift = 0.04;
        settings.highlight_compression = 0.08;
        settings.tone_strength = 0.30;
    }

    if (!initialized) {
        smoothed_gain = proposed_gain;
        smoothed_gamma = proposed_gamma;
        smoothed_denoise = proposed_denoise;
        smoothed_sharpen = proposed_sharpen;
        initialized = true;
    } else {
        smoothed_gain = smooth_value(smoothed_gain, proposed_gain, 0.18);
        smoothed_gamma = smooth_value(smoothed_gamma, proposed_gamma, 0.16);
        smoothed_denoise = smooth_value(smoothed_denoise, proposed_denoise, 0.22);
        smoothed_sharpen = smooth_value(smoothed_sharpen, proposed_sharpen, 0.22);
    }

    settings.brightness_gain = clamp_double(smoothed_gain, 0.60, 1.65);
    settings.gamma = clamp_double(smoothed_gamma, 0.65, 1.35);
    settings.denoise_strength = clamp_double(smoothed_denoise, 0.0, 0.70);
    settings.sharpen_amount = clamp_double(smoothed_sharpen, 0.0, 0.45);

    if (contrast < 45.0) {
        settings.contrast_alpha = 1.08;
    } else if (contrast > 95.0) {
        settings.contrast_alpha = 0.96;
    } else {
        settings.contrast_alpha = 1.02;
    }

    settings.contrast_beta = 0.0;

    return settings;
}

Frame apply_isp_tuning(const Frame& frame, const IspSettings& settings) {
    cv::Mat image = frame_to_mat(frame);

    if (image.empty()) {
        return frame;
    }

    cv::Mat wb_input = image.clone();
    cv::Scalar means = channel_means(wb_input);

    double b_mean = std::max(1.0, means[0]);
    double g_mean = std::max(1.0, means[1]);
    double r_mean = std::max(1.0, means[2]);

    IspSettings applied = settings;
    applied.blue_gain = clamp_double(g_mean / b_mean, 0.82, 1.22);
    applied.green_gain = 1.0;
    applied.red_gain = clamp_double(g_mean / r_mean, 0.82, 1.22);

    cv::Mat output = apply_white_balance(image, applied);

    output.convertTo(output, -1, applied.brightness_gain * applied.contrast_alpha, applied.contrast_beta);

    output = apply_gamma_curve(output, applied.gamma);
    output = apply_tone_curve(output, applied);

    if (applied.clahe_enabled) {
        cv::Mat lab;
        cv::cvtColor(output, lab, cv::COLOR_BGR2Lab);

        std::vector<cv::Mat> channels;
        cv::split(lab, channels);

        cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(2.0, cv::Size(8, 8));
        clahe->apply(channels[0], channels[0]);

        cv::merge(channels, lab);
        cv::cvtColor(lab, output, cv::COLOR_Lab2BGR);
    }

    output = apply_denoise(output, applied.denoise_strength);
    output = apply_sharpen(output, applied.sharpen_amount);

    return mat_to_frame(output, frame.id);
}