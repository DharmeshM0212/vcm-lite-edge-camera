#pragma once

#include "frame.hpp"
#include "roi.hpp"

#include <optional>
#include <vector>

struct MotionRoiConfig {
    double threshold;
    double min_area_ratio;
    double max_area_ratio;
    double merge_iou_threshold;
    int max_rois;
    int tracking_hold_frames;
    double smoothing_alpha;
};

class MotionRoiDetector {
public:
    MotionRoiDetector();

    RoiResult detect(const Frame& frame);

private:
    MotionRoiConfig config_;
    std::optional<Frame> previous_frame_;
    RoiResult tracked_result_;
    int missing_frames_;

    RoiResult detect_motion(const Frame& frame);
    RoiResult smooth_with_previous(const RoiResult& current, int frame_width, int frame_height);
};