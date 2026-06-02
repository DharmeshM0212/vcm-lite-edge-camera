#include "semantic_encoder.hpp"

SemanticEncodeResult estimate_semantic_encode(const Frame& frame, const RoiResult& roi_result) {
    double roi_area = 0.0;

    for (const auto& box : roi_result.boxes) {
        roi_area += static_cast<double>(box.width * box.height);
    }

    double frame_area = static_cast<double>(frame.width * frame.height);
    double roi_ratio = frame_area > 0.0 ? roi_area / frame_area : 0.0;

    SemanticEncodeResult result;
    result.roi_quality = 90;
    result.context_quality = 35;
    result.estimated_bitrate_kbps = 250.0 + 1200.0 * roi_ratio + 4.0 * result.context_quality + 3.0 * result.roi_quality;
    return result;
}