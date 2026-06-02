#include "config.hpp"
#include "frame.hpp"
#include "frame_queue.hpp"
#include "image_statistics.hpp"
#include "isp_tuning.hpp"
#include "metrics.hpp"
#include "roi.hpp"
#include "semantic_encoder.hpp"
#include "video_source.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>
#include <thread>

int main(int argc, char** argv) {
    std::string video_path = "../assets/videos/input.mp4";

    if (argc >= 2) {
        video_path = argv[1];
    }

    VideoSource source(video_path);

    if (!source.is_opened()) {
        std::cerr << "failed_to_open_video:" << video_path << std::endl;
        return 1;
    }

    EngineConfig config = default_config();
    FrameQueue queue(3);
    std::atomic<bool> running(true);

    double source_fps = source.fps();

    if (source_fps <= 1.0 || source_fps > 240.0) {
        source_fps = 30.0;
    }

    int source_delay_ms = static_cast<int>(1000.0 / source_fps);

    std::thread producer([&]() {
        while (running.load()) {
            auto maybe_frame = source.read();

            if (!maybe_frame.has_value()) {
                running.store(false);
                break;
            }

            queue.push(std::move(*maybe_frame));
            std::this_thread::sleep_for(std::chrono::milliseconds(source_delay_ms));
        }
    });

    std::thread consumer([&]() {
        auto fps_window_start = std::chrono::steady_clock::now();
        std::uint64_t fps_window_count = 0;
        double measured_fps = 0.0;

        while (running.load() || queue.size() > 0) {
            auto maybe_frame = queue.pop();

            if (maybe_frame.has_value()) {
                fps_window_count++;
                auto fps_now = std::chrono::steady_clock::now();
                auto fps_elapsed = std::chrono::duration<double>(fps_now - fps_window_start).count();

                if (fps_elapsed >= 1.0) {
                    measured_fps = static_cast<double>(fps_window_count) / fps_elapsed;
                    fps_window_count = 0;
                    fps_window_start = fps_now;
                }

                auto now = std::chrono::steady_clock::now();

                ImageStats input_stats = compute_image_stats(*maybe_frame);
                double brightness_gain = choose_brightness_gain(input_stats.mean_brightness);
                double gamma = choose_gamma(input_stats.mean_brightness);
                Frame gain_frame = apply_brightness_gain(*maybe_frame, brightness_gain);
                Frame tuned_frame = apply_gamma_correction(gain_frame, gamma);
                ImageStats stats = compute_image_stats(tuned_frame);
                RoiResult roi_result = detect_synthetic_roi(tuned_frame);

                double roi_area = 0.0;

                for (const auto& box : roi_result.boxes) {
                    roi_area += static_cast<double>(box.width * box.height);
                }

                double frame_area = static_cast<double>(tuned_frame.width * tuned_frame.height);
                double roi_area_ratio = frame_area > 0.0 ? roi_area / frame_area : 0.0;

                SemanticEncodeResult encode_result = estimate_semantic_encode(tuned_frame, roi_result);

                EngineMetrics metrics;
                metrics.frame_id = maybe_frame->id;
                metrics.fps = measured_fps;
                metrics.latency_ms = std::chrono::duration<double, std::milli>(now - maybe_frame->created_at).count();
                metrics.bitrate_kbps = encode_result.estimated_bitrate_kbps;
                metrics.mean_brightness = stats.mean_brightness;
                metrics.brightness_gain = brightness_gain;
                metrics.gamma = gamma;
                metrics.roi_count = static_cast<std::uint32_t>(roi_result.boxes.size());
                metrics.roi_area_ratio = roi_area_ratio;
                metrics.roi_quality = static_cast<std::uint32_t>(encode_result.roi_quality);
                metrics.context_quality = static_cast<std::uint32_t>(encode_result.context_quality);
                metrics.ai_stability_loss = 0.0;
                metrics.cpu_percent = 0.0;
                metrics.ram_mb = static_cast<double>(tuned_frame.data.size()) / (1024.0 * 1024.0);
                metrics.dropped_frames = static_cast<std::uint32_t>(queue.dropped_count());
                metrics.queue_depth = static_cast<std::uint32_t>(queue.size());
                metrics.mode = config.mode;

                std::cout << metrics_to_json(metrics) << std::endl;
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(40));
        }
    });

    producer.join();
    consumer.join();

    return 0;
}