#include "metrics.hpp"

#include <iomanip>
#include <sstream>

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
    ss << "\"isp_profile\":\"" << metrics.isp_profile << "\",";
    ss << "\"roi_count\":" << metrics.roi_count << ",";
    ss << "\"roi_area_ratio\":" << metrics.roi_area_ratio << ",";
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
    ss << "\"controller_state\":\"" << metrics.controller_state << "\",";
    ss << "\"ai_stability_loss\":" << metrics.ai_stability_loss << ",";
    ss << "\"cpu_percent\":" << metrics.cpu_percent << ",";
    ss << "\"ram_mb\":" << metrics.ram_mb << ",";
    ss << "\"dropped_frames\":" << metrics.dropped_frames << ",";
    ss << "\"queue_depth\":" << metrics.queue_depth << ",";
    ss << "\"mode\":\"" << metrics.mode << "\"";
    ss << "}";
    return ss.str();
}