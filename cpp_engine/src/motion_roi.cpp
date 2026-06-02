#include "motion_roi.hpp"

#include "opencv_bridge.hpp"

#include <algorithm>
#include <opencv2/opencv.hpp>

MotionRoiDetector::MotionRoiDetector()
    : missing_frames_(0) {
    config_.threshold = 18.0;
    config_.min_area_ratio = 0.0015;
    config_.max_area_ratio = 0.70;
    config_.merge_iou_threshold = 0.15;
    config_.max_rois = 5;
    config_.tracking_hold_frames = 8;
    config_.smoothing_alpha = 0.65;
}

RoiResult MotionRoiDetector::detect(const Frame& frame) {
    RoiResult detected = detect_motion(frame);

    if (!detected.boxes.empty()) {
        RoiResult merged = merge_overlapping_rois(detected, config_.merge_iou_threshold, frame.width, frame.height);
        RoiResult limited = limit_rois(merged, config_.max_rois);
        RoiResult smoothed = smooth_with_previous(limited, frame.width, frame.height);
        tracked_result_ = smoothed;
        missing_frames_ = 0;
        previous_frame_ = frame;
        return smoothed;
    }

    previous_frame_ = frame;

    if (!tracked_result_.boxes.empty() && missing_frames_ < config_.tracking_hold_frames) {
        missing_frames_++;

        RoiResult held = tracked_result_;

        for (auto& box : held.boxes) {
            box.confidence *= 0.85;
        }

        tracked_result_ = held;
        return held;
    }

    missing_frames_ = 0;
    tracked_result_.boxes.clear();
    return RoiResult{};
}

RoiResult MotionRoiDetector::detect_motion(const Frame& frame) {
    RoiResult result;

    if (!previous_frame_.has_value()) {
        previous_frame_ = frame;
        return result;
    }

    cv::Mat current = frame_to_mat(frame);
    cv::Mat previous = frame_to_mat(*previous_frame_);

    if (current.empty() || previous.empty()) {
        return result;
    }

    if (current.size() != previous.size()) {
        previous_frame_ = frame;
        return result;
    }

    cv::Mat current_gray;
    cv::Mat previous_gray;

    if (current.channels() == 1) {
        current_gray = current;
    } else {
        cv::cvtColor(current, current_gray, cv::COLOR_BGR2GRAY);
    }

    if (previous.channels() == 1) {
        previous_gray = previous;
    } else {
        cv::cvtColor(previous, previous_gray, cv::COLOR_BGR2GRAY);
    }

    cv::Mat current_blur;
    cv::Mat previous_blur;
    cv::GaussianBlur(current_gray, current_blur, cv::Size(5, 5), 0.0);
    cv::GaussianBlur(previous_gray, previous_blur, cv::Size(5, 5), 0.0);

    cv::Mat diff;
    cv::absdiff(current_blur, previous_blur, diff);

    cv::Mat mask;
    cv::threshold(diff, mask, config_.threshold, 255.0, cv::THRESH_BINARY);

    cv::Mat kernel_small = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
    cv::Mat kernel_large = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(7, 7));

    cv::morphologyEx(mask, mask, cv::MORPH_OPEN, kernel_small);
    cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, kernel_large);
    cv::dilate(mask, mask, kernel_large, cv::Point(-1, -1), 1);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    double frame_area = static_cast<double>(frame.width * frame.height);

    for (const auto& contour : contours) {
        double area = cv::contourArea(contour);
        double area_ratio = frame_area > 0.0 ? area / frame_area : 0.0;

        if (area_ratio < config_.min_area_ratio || area_ratio > config_.max_area_ratio) {
            continue;
        }

        cv::Rect rect = cv::boundingRect(contour);

        RoiBox box;
        box.x = rect.x;
        box.y = rect.y;
        box.width = rect.width;
        box.height = rect.height;
        box.confidence = std::clamp(area_ratio / 0.08, 0.05, 1.0);

        result.boxes.push_back(clamp_roi_box(box, frame.width, frame.height));
    }

    return result;
}

RoiResult MotionRoiDetector::smooth_with_previous(const RoiResult& current, int frame_width, int frame_height) {
    if (tracked_result_.boxes.empty()) {
        return current;
    }

    RoiResult smoothed = current;

    for (auto& box : smoothed.boxes) {
        double best_iou = 0.0;
        const RoiBox* best_previous = nullptr;

        for (const auto& prev : tracked_result_.boxes) {
            double current_iou = roi_iou(box, prev);

            if (current_iou > best_iou) {
                best_iou = current_iou;
                best_previous = &prev;
            }
        }

        if (best_previous != nullptr && best_iou > 0.05) {
            double a = config_.smoothing_alpha;

            box.x = static_cast<int>(a * static_cast<double>(box.x) + (1.0 - a) * static_cast<double>(best_previous->x));
            box.y = static_cast<int>(a * static_cast<double>(box.y) + (1.0 - a) * static_cast<double>(best_previous->y));
            box.width = static_cast<int>(a * static_cast<double>(box.width) + (1.0 - a) * static_cast<double>(best_previous->width));
            box.height = static_cast<int>(a * static_cast<double>(box.height) + (1.0 - a) * static_cast<double>(best_previous->height));
            box.confidence = std::max(box.confidence, best_previous->confidence * 0.92);
            box = clamp_roi_box(box, frame_width, frame_height);
        }
    }

    return limit_rois(smoothed, config_.max_rois);
}