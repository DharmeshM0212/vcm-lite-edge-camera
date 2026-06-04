#pragma once

#include "config.hpp"

#include <cstdint>
#include <string>

struct RateControllerInput {
    double latency_ms;
    double roi_area_ratio;
    double estimated_fps;
    double previous_bitrate_kbps;
    double previous_ai_stability_loss;
    std::uint32_t queue_depth;
    std::uint32_t dropped_frames;
};

struct RateControllerOutput {
    int roi_quality;
    int context_quality;
    int detector_interval;
    int context_width;
    int roi_cell_size;
    int max_rois;
    bool reencode_allowed;
    std::string controller_state;
    std::string controller_mode;
    std::string controller_reason;
    std::string controller_action;
};

class RateController {
public:
    explicit RateController(const EngineConfig& config);

    RateControllerOutput update(const RateControllerInput& input);

private:
    EngineConfig config_;
    int last_roi_quality_;
    int last_context_quality_;
    int last_detector_interval_;
    int last_context_width_;
    int last_roi_cell_size_;
    int last_max_rois_;

    int clamp_int(int value, int low, int high) const;
};