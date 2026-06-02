#include "image_statistics.hpp"

#include "opencv_bridge.hpp"

#include <opencv2/opencv.hpp>

ImageStats compute_image_stats(const Frame& frame) {
    cv::Mat input = frame_to_mat(frame);
    cv::Mat gray;

    if (input.channels() == 1) {
        gray = input;
    } else {
        cv::cvtColor(input, gray, cv::COLOR_BGR2GRAY);
    }

    cv::Scalar mean_value;
    cv::Scalar stddev_value;
    cv::meanStdDev(gray, mean_value, stddev_value);

    cv::Mat laplacian;
    cv::Laplacian(gray, laplacian, CV_64F);

    cv::Scalar lap_mean;
    cv::Scalar lap_stddev;
    cv::meanStdDev(laplacian, lap_mean, lap_stddev);

    cv::Mat blurred;
    cv::GaussianBlur(gray, blurred, cv::Size(3, 3), 0.0);

    cv::Mat residual;
    cv::absdiff(gray, blurred, residual);

    cv::Scalar noise_mean = cv::mean(residual);

    ImageStats stats;
    stats.mean_brightness = mean_value[0];
    stats.contrast = stddev_value[0];
    stats.sharpness = lap_stddev[0] * lap_stddev[0];
    stats.noise = noise_mean[0];
    return stats;
}