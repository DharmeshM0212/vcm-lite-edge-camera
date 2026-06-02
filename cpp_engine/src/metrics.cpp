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
    ss << "\"mean_brightness\":" << metrics.mean_brightness << ",";
    ss << "\"brightness_gain\":" << metrics.brightness_gain << ",";
    ss << "\"gamma\":" << metrics.gamma << ",";
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