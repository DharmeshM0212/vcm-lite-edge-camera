#include "metadata.hpp"

#include <iomanip>
#include <sstream>

std::string metadata_to_json(const MetadataPacketInput& input) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(3);
    ss << "{";
    ss << "\"frame_id\":" << input.frame_id << ",";
    ss << "\"latency_ms\":" << input.latency_ms << ",";
    ss << "\"bitrate_kbps\":" << input.bitrate_kbps << ",";
    ss << "\"ai_stability_loss\":" << input.ai_stability_loss << ",";
    ss << "\"context_width\":" << input.context_width << ",";
    ss << "\"context_height\":" << input.context_height << ",";
    ss << "\"roi_tile_width\":" << input.roi_tile_width << ",";
    ss << "\"roi_tile_height\":" << input.roi_tile_height << ",";
    ss << "\"roi_quality\":" << input.roi_quality << ",";
    ss << "\"context_quality\":" << input.context_quality << ",";
    ss << "\"tile_cols\":" << input.tile_cols << ",";
    ss << "\"tile_rows\":" << input.tile_rows << ",";
    ss << "\"cell_width\":" << input.cell_width << ",";
    ss << "\"cell_height\":" << input.cell_height << ",";
    ss << "\"reencoded\":" << (input.reencoded ? "true" : "false") << ",";
    ss << "\"controller_state\":\"" << input.controller_state << "\",";
    ss << "\"rois\":[";

    for (std::size_t i = 0; i < input.roi_result.boxes.size(); i++) {
        const RoiBox& box = input.roi_result.boxes[i];

        int tile_col = input.tile_cols > 0 ? static_cast<int>(i) % input.tile_cols : 0;
        int tile_row = input.tile_cols > 0 ? static_cast<int>(i) / input.tile_cols : 0;

        ss << "{";
        ss << "\"id\":" << i << ",";
        ss << "\"x\":" << box.x << ",";
        ss << "\"y\":" << box.y << ",";
        ss << "\"width\":" << box.width << ",";
        ss << "\"height\":" << box.height << ",";
        ss << "\"confidence\":" << box.confidence << ",";
        ss << "\"tile_x\":" << tile_col * input.cell_width << ",";
        ss << "\"tile_y\":" << tile_row * input.cell_height << ",";
        ss << "\"tile_width\":" << input.cell_width << ",";
        ss << "\"tile_height\":" << input.cell_height;
        ss << "}";

        if (i + 1 < input.roi_result.boxes.size()) {
            ss << ",";
        }
    }

    ss << "]";
    ss << "}";
    return ss.str();
}