#include "metrics.hpp"

#include <iomanip>
#include <sstream>
#include <string>

static std::string json_escape(const std::string& text) {
    std::ostringstream output;

    for (char c : text) {
        switch (c) {
            case '"':
                output << "\\\"";
                break;
            case '\\':
                output << "\\\\";
                break;
            case '\n':
                output << "\\n";
                break;
            case '\r':
                output << "\\r";
                break;
            case '\t':
                output << "\\t";
                break;
            default:
                output << c;
                break;
        }
    }

    return output.str();
}

std::string metrics_to_json(const EngineMetrics& metrics) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(3);

    ss << "{";
    ss << "\"frame_id\":" << metrics.frame_id << ",";
    ss << "\"fps\":" << metrics.fps << ",";
    ss << "\"latency_ms\":" << metrics.latency_ms << ",";
    ss << "\"bitrate_kbps\":" << metrics.bitrate_kbps << ",";

    ss << "\"input_brightness\":" << metrics.input_brightness << ",";
    ss << "\"input_contrast\":" << metrics.input_contrast << ",";
    ss << "\"input_sharpness\":" << metrics.input_sharpness << ",";
    ss << "\"input_noise\":" << metrics.input_noise << ",";

    ss << "\"mean_brightness\":" << metrics.mean_brightness << ",";
    ss << "\"output_contrast\":" << metrics.output_contrast << ",";
    ss << "\"output_sharpness\":" << metrics.output_sharpness << ",";
    ss << "\"output_noise\":" << metrics.output_noise << ",";

    ss << "\"brightness_gain\":" << metrics.brightness_gain << ",";
    ss << "\"gamma\":" << metrics.gamma << ",";
    ss << "\"contrast_alpha\":" << metrics.contrast_alpha << ",";
    ss << "\"contrast_beta\":" << metrics.contrast_beta << ",";
    ss << "\"denoise_strength\":" << metrics.denoise_strength << ",";
    ss << "\"sharpen_amount\":" << metrics.sharpen_amount << ",";
    ss << "\"clahe_enabled\":" << (metrics.clahe_enabled ? "true" : "false") << ",";
    ss << "\"isp_profile\":\"" << json_escape(metrics.isp_profile) << "\",";

    ss << "\"roi_count\":" << metrics.roi_count << ",";
    ss << "\"roi_area_ratio\":" << metrics.roi_area_ratio << ",";
    ss << "\"detected_object_count\":" << metrics.detected_object_count << ",";
    ss << "\"detector_ran\":" << (metrics.detector_ran ? "true" : "false") << ",";
    ss << "\"detector_used_dnn\":" << (metrics.detector_used_dnn ? "true" : "false") << ",";
    ss << "\"raw_detector_candidates\":" << metrics.raw_detector_candidates << ",";
    ss << "\"max_detector_confidence\":" << metrics.max_detector_confidence << ",";
    ss << "\"reference_ai_confidence\":" << metrics.reference_ai_confidence << ",";
    ss << "\"compressed_ai_confidence\":" << metrics.compressed_ai_confidence << ",";
    ss << "\"detector_confidence_loss\":" << metrics.detector_confidence_loss << ",";

    ss << "\"roi_quality\":" << metrics.roi_quality << ",";
    ss << "\"context_quality\":" << metrics.context_quality << ",";
    ss << "\"context_width\":" << metrics.context_width << ",";
    ss << "\"context_height\":" << metrics.context_height << ",";
    ss << "\"roi_tile_width\":" << metrics.roi_tile_width << ",";
    ss << "\"roi_tile_height\":" << metrics.roi_tile_height << ",";
    ss << "\"context_jpeg_bytes\":" << metrics.context_jpeg_bytes << ",";
    ss << "\"roi_tile_jpeg_bytes\":" << metrics.roi_tile_jpeg_bytes << ",";
    ss << "\"total_encoded_bytes\":" << metrics.total_encoded_bytes << ",";

    ss << "\"detector_interval\":" << metrics.detector_interval << ",";
    ss << "\"reencode_allowed\":" << (metrics.reencode_allowed ? "true" : "false") << ",";
    ss << "\"reencoded\":" << (metrics.reencoded ? "true" : "false") << ",";
    ss << "\"reencode_attempts\":" << metrics.reencode_attempts << ",";
    ss << "\"initial_roi_quality\":" << metrics.initial_roi_quality << ",";
    ss << "\"final_roi_quality\":" << metrics.final_roi_quality << ",";
    ss << "\"initial_ai_stability_loss\":" << metrics.initial_ai_stability_loss << ",";

    ss << "\"controller_state\":\"" << json_escape(metrics.controller_state) << "\",";
    ss << "\"controller_mode\":\"" << json_escape(metrics.controller_mode) << "\",";
    ss << "\"controller_reason\":\"" << json_escape(metrics.controller_reason) << "\",";
    ss << "\"controller_action\":\"" << json_escape(metrics.controller_action) << "\",";
    ss << "\"ai_stability_loss\":" << metrics.ai_stability_loss << ",";

    ss << "\"cpu_percent\":" << metrics.cpu_percent << ",";
    ss << "\"ram_mb\":" << metrics.ram_mb << ",";
    ss << "\"dropped_frames\":" << metrics.dropped_frames << ",";
    ss << "\"queue_depth\":" << metrics.queue_depth << ",";
    ss << "\"mode\":\"" << json_escape(metrics.mode) << "\"";
    ss << "}";

    return ss.str();
}