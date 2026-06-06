#pragma once

#include "frame.hpp"

#include <vector>

struct RoiBox {
    int x;
    int y;
    int width;
    int height;
    double confidence;
};

struct RoiResult {
    std::vector<RoiBox> boxes;
};

RoiResult detect_synthetic_roi(const Frame& frame);
double roi_area_ratio(const Frame& frame, const RoiResult& roi_result);
RoiBox clamp_roi_box(const RoiBox& box, int frame_width, int frame_height);
double roi_iou(const RoiBox& a, const RoiBox& b);
RoiResult merge_overlapping_rois(const RoiResult& input, double iou_threshold, int frame_width, int frame_height);
RoiResult limit_rois(const RoiResult& input, int max_rois);
RoiResult limit_rois_by_area_budget(const RoiResult& input, int max_rois, int frame_width, int frame_height, double max_area_ratio);