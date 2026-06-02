#include "config.hpp"
#include "frame.hpp"
#include "frame_queue.hpp"
#include "metrics.hpp"
#include "image_statistics.hpp"
#include "isp_tuning.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>

int main() {
    EngineConfig config = default_config();
    FrameQueue queue(3);
    std::atomic<bool> running(true);
    std::atomic<std::uint64_t> produced_frames(0);
    std::atomic<std::uint64_t> consumed_frames(0);

    std::thread producer([&]() {
        std::uint64_t frame_id = 0;

        while (running.load()) {
            Frame frame = make_synthetic_frame(frame_id, 640, 360, 3);
            queue.push(std::move(frame));
            produced_frames.store(frame_id + 1);
            frame_id++;
            std::this_thread::sleep_for(std::chrono::milliseconds(33));
        }
    });

        std::thread consumer([&]() {
        auto fps_window_start = std::chrono::steady_clock::now();
        std::uint64_t fps_window_count = 0;
        double measured_fps = 0.0;

        while (running.load()) {
            auto maybe_frame = queue.pop();

            if (maybe_frame.has_value()) {
                consumed_frames.store(maybe_frame->id + 1);
                fps_window_count++;
                auto fps_now = std::chrono::steady_clock::now();
                auto fps_elapsed = std::chrono::duration<double>(fps_now - fps_window_start).count();

                if (fps_elapsed >= 1.0) {
                    measured_fps = static_cast<double>(fps_window_count) / fps_elapsed;
                    fps_window_count = 0;
                    fps_window_start = fps_now;
                }
                ImageStats input_stats = compute_image_stats(*maybe_frame);
                double brightness_gain = choose_brightness_gain(input_stats.mean_brightness);
                Frame tuned_frame = apply_brightness_gain(*maybe_frame, brightness_gain);
                ImageStats stats = compute_image_stats(tuned_frame);
                EngineMetrics metrics;
                metrics.frame_id = maybe_frame->id;
                metrics.fps = measured_fps;
                auto now = std::chrono::steady_clock::now();
                metrics.latency_ms = std::chrono::duration<double, std::milli>(now - maybe_frame->created_at).count();
                metrics.bitrate_kbps = 0.0;
                metrics.mean_brightness = stats.mean_brightness;
                metrics.brightness_gain = brightness_gain;
                metrics.ai_stability_loss = 0.0;
                metrics.cpu_percent = 0.0;
                metrics.ram_mb = static_cast<double>(tuned_frame.data.size()) / (1024.0 * 1024.0);
                metrics.dropped_frames = static_cast<std::uint32_t>(queue.dropped_count());
                metrics.queue_depth = static_cast<std::uint32_t>(queue.size());
                metrics.mode = config.mode;

                std::cout << metrics_to_json(metrics) << std::endl;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });

    std::this_thread::sleep_for(std::chrono::seconds(20));
    running.store(false);

    producer.join();
    consumer.join();

    return 0;
}