#pragma once

#include <cstdint>
#include <string>

struct EngineMetrics {
    std::uint64_t frame_id;
    double fps;
    double latency_ms;
    double bitrate_kbps;
    double input_brightness;
    double input_contrast;
    double input_sharpness;
    double input_noise;
    double mean_brightness;
    double output_contrast;
    double output_sharpness;
    double output_noise;
    double brightness_gain;
    double gamma;
    double contrast_alpha;
    double contrast_beta;
    double denoise_strength;
    double sharpen_amount;
    bool clahe_enabled;
    std::string isp_profile;
    std::uint32_t roi_count;
    double roi_area_ratio;
    std::uint32_t roi_quality;
    std::uint32_t context_quality;
    std::uint32_t context_width;
    std::uint32_t context_height;
    std::uint32_t roi_tile_width;
    std::uint32_t roi_tile_height;
    std::uint32_t context_jpeg_bytes;
    std::uint32_t roi_tile_jpeg_bytes;
    std::uint32_t total_encoded_bytes;
    std::uint32_t detector_interval;
    bool reencode_allowed;
    std::string controller_state;
    double ai_stability_loss;
    double cpu_percent;
    double ram_mb;
    std::uint32_t dropped_frames;
    std::uint32_t queue_depth;
    std::string mode;
};

std::string metrics_to_json(const EngineMetrics& metrics);