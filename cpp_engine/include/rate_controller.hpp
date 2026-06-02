#pragma once

#include "config.hpp"

#include <cstdint>
#include <string>

struct RateControllerInput {
    double latency_ms;
    double roi_area_ratio;
    double estimated_fps;
    double previous_bitrate_kbps;
    std::uint32_t queue_depth;
    std::uint32_t dropped_frames;
};

struct RateControllerOutput {
    int roi_quality;
    int context_quality;
    int detector_interval;
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
};