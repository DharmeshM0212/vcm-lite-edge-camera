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
    ss << "\"ai_stability_loss\":" << metrics.ai_stability_loss << ",";
    ss << "\"cpu_percent\":" << metrics.cpu_percent << ",";
    ss << "\"ram_mb\":" << metrics.ram_mb << ",";
    ss << "\"dropped_frames\":" << metrics.dropped_frames << ",";
    ss << "\"queue_depth\":" << metrics.queue_depth << ",";
    ss << "\"mode\":\"" << metrics.mode << "\"";
    ss << "}";
    return ss.str();
}