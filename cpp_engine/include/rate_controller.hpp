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
    bool initialized_;
    std::uint32_t previous_dropped_frames_;

    RateControllerOutput balanced_output(const RateControllerInput& input) const;
    RateControllerOutput sparse_idle_output(const RateControllerInput& input) const;
    RateControllerOutput realtime_protect_output(const RateControllerInput& input) const;
    RateControllerOutput dense_roi_output(const RateControllerInput& input) const;
    RateControllerOutput overload_output(const RateControllerInput& input) const;
};