#pragma once

#include <cstdint>
#include <string>

struct EngineMetrics {
    std::uint64_t frame_id;

    double fps;
    double latency_ms;
    double bitrate_kbps;

    double queue_wait_ms;
    double processing_ms;
    double isp_ms;
    double motion_roi_ms;
    double detector_ms;
    double semantic_encode_ms;
    double semantic_reconstruct_ms;
    double compressed_validation_ms;
    double event_write_ms;

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

    std::uint32_t detected_object_count;
    bool detector_ran;
    bool detector_used_dnn;
    std::uint32_t raw_detector_candidates;
    double max_detector_confidence;

    double reference_ai_confidence;
    double compressed_ai_confidence;
    double detector_confidence_loss;

    double semantic_psnr_db;
    double semantic_ssim;
    double task_preservation_loss;

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
    bool reencoded;
    std::uint32_t reencode_attempts;
    std::uint32_t initial_roi_quality;
    std::uint32_t final_roi_quality;
    double initial_ai_stability_loss;

    std::string controller_state;
    std::string controller_mode;
    std::string controller_reason;
    std::string controller_action;

    double ai_stability_loss;

    double cpu_percent;
    double ram_mb;
    std::uint32_t dropped_frames;
    std::uint32_t queue_depth;

    std::string mode;
};

std::string metrics_to_json(const EngineMetrics& metrics);