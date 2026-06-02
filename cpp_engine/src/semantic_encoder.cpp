#include "semantic_encoder.hpp"

SemanticEncodeResult estimate_semantic_encode(const Frame& frame, const RoiResult& roi_result, int roi_quality, int context_quality) {
    double roi_area = 0.0;

    for (const auto& box : roi_result.boxes) {
        roi_area += static_cast<double>(box.width * box.height);
    }

    double frame_area = static_cast<double>(frame.width * frame.height);
    double roi_ratio = frame_area > 0.0 ? roi_area / frame_area : 0.0;

    SemanticEncodeResult result;
    result.roi_quality = roi_quality;
    result.context_quality = context_quality;
    result.estimated_bitrate_kbps = 160.0 + 1000.0 * roi_ratio + 3.0 * context_quality + 2.0 * roi_quality;
    return result;
}