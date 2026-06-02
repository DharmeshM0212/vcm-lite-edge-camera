#include "motion_roi.hpp"

#include "opencv_bridge.hpp"

#include <algorithm>
#include <opencv2/opencv.hpp>

MotionRoiDetector::MotionRoiDetector() {
}

RoiResult MotionRoiDetector::detect(const Frame& frame) {
    RoiResult result;

    if (!previous_frame_.has_value()) {
        previous_frame_ = frame;
        return result;
    }

    cv::Mat current = frame_to_mat(frame);
    cv::Mat previous = frame_to_mat(*previous_frame_);

    cv::Mat current_gray;
    cv::Mat previous_gray;

    cv::cvtColor(current, current_gray, cv::COLOR_BGR2GRAY);
    cv::cvtColor(previous, previous_gray, cv::COLOR_BGR2GRAY);

    cv::Mat diff;
    cv::absdiff(current_gray, previous_gray, diff);

    cv::Mat blurred;
    cv::GaussianBlur(diff, blurred, cv::Size(5, 5), 0.0);

    cv::Mat mask;
    cv::threshold(blurred, mask, 20.0, 255.0, cv::THRESH_BINARY);

    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(5, 5));
    cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, kernel);
    cv::dilate(mask, mask, kernel, cv::Point(-1, -1), 1);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    for (const auto& contour : contours) {
        double area = cv::contourArea(contour);

        if (area < 500.0) {
            continue;
        }

        cv::Rect rect = cv::boundingRect(contour);

        RoiBox box;
        box.x = std::clamp(rect.x, 0, frame.width - 1);
        box.y = std::clamp(rect.y, 0, frame.height - 1);
        box.width = std::clamp(rect.width, 1, frame.width - box.x);
        box.height = std::clamp(rect.height, 1, frame.height - box.y);
        box.confidence = std::min(1.0, area / static_cast<double>(frame.width * frame.height));

        result.boxes.push_back(box);
    }

    std::sort(result.boxes.begin(), result.boxes.end(), [](const RoiBox& a, const RoiBox& b) {
        return a.width * a.height > b.width * b.height;
    });

    if (result.boxes.size() > 5) {
        result.boxes.resize(5);
    }

    previous_frame_ = frame;
    return result;
}