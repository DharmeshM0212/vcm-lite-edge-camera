#include "config.hpp"

EngineConfig default_config() {
    EngineConfig config;
    config.latency_budget_ms = 100.0;
    config.confidence_loss_threshold = 0.08;
    config.target_bitrate_kbps = 600;
    config.roi_quality_min = 60;
    config.roi_quality_max = 95;
    config.context_quality_min = 20;
    config.context_quality_max = 60;
    config.detector_interval = 5;
    config.mode = "heartbeat";
    return config;
}