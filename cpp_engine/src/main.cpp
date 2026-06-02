#include "config.hpp"
#include "frame.hpp"
#include "frame_queue.hpp"
#include "metrics.hpp"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>

int main() {
    EngineConfig config = default_config();
    FrameQueue queue(3);
    std::uint64_t frame_id = 0;

    while (true) {
        Frame frame = make_empty_frame(frame_id, 640, 360, 3);
        queue.push(std::move(frame));

        auto maybe_frame = queue.pop();

        EngineMetrics metrics;
        metrics.frame_id = frame_id;
        metrics.fps = 30.0;
        metrics.latency_ms = 12.0;
        metrics.bitrate_kbps = 0.0;
        metrics.ai_stability_loss = 0.0;
        metrics.cpu_percent = 0.0;
        metrics.ram_mb = 0.0;
        metrics.dropped_frames = static_cast<std::uint32_t>(queue.dropped_count());
        metrics.queue_depth = static_cast<std::uint32_t>(queue.size());
        metrics.mode = config.mode;

        if (maybe_frame.has_value()) {
            metrics.frame_id = maybe_frame->id;
            metrics.ram_mb = static_cast<double>(maybe_frame->data.size()) / (1024.0 * 1024.0);
        }

        std::cout << metrics_to_json(metrics) << std::endl;

        frame_id++;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}