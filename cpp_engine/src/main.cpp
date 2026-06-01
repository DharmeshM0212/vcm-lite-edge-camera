#include "config.hpp"
#include "metrics.hpp"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>

int main() {
    EngineConfig config = default_config();
    std::uint64_t frame_id = 0;

    while (true) {
        EngineMetrics metrics;
        metrics.frame_id = frame_id;
        metrics.fps = 30.0;
        metrics.latency_ms = 12.0;
        metrics.bitrate_kbps = 0.0;
        metrics.ai_stability_loss = 0.0;
        metrics.cpu_percent = 0.0;
        metrics.ram_mb = 0.0;
        metrics.dropped_frames = 0;
        metrics.queue_depth = 0;
        metrics.mode = config.mode;

        std::cout << metrics_to_json(metrics) << std::endl;

        frame_id++;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}