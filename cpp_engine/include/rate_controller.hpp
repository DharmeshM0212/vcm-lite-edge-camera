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
};

class RateController {
public:
    explicit RateController(const EngineConfig& config);

    RateControllerOutput update(const RateControllerInput& input);

private:
    EngineConfig config_;
    int roi_quality_;
    int context_quality_;
    int detector_interval_;
    int context_width_;
    int roi_cell_size_;
    int max_rois_;
    std::uint32_t previous_dropped_frames_;

    double bitrate_error_ratio(double bitrate_kbps) const;
    bool is_overloaded(const RateControllerInput& input) const;
    bool is_ai_unstable(const RateControllerInput& input) const;
    bool is_rate_limited(const RateControllerInput& input) const;
};