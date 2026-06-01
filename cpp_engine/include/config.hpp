#pragma once

#include <string>

struct EngineConfig {
    double latency_budget_ms;
    double confidence_loss_threshold;
    int target_bitrate_kbps;
    int roi_quality_min;
    int roi_quality_max;
    int context_quality_min;
    int context_quality_max;
    int detector_interval;
    std::string mode;
};

EngineConfig default_config();