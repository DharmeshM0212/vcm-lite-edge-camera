#pragma once

#include <cstdint>
#include <string>

struct EngineMetrics {
    std::uint64_t frame_id;
    double fps;
    double latency_ms;
    double bitrate_kbps;
    double ai_stability_loss;
    double cpu_percent;
    double ram_mb;
    std::uint32_t dropped_frames;
    std::uint32_t queue_depth;
    std::string mode;
};

std::string metrics_to_json(const EngineMetrics& metrics);