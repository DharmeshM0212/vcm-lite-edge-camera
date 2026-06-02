#pragma once

#include "roi.hpp"

#include <cstdint>
#include <string>

struct MetadataPacketInput {
    std::uint64_t frame_id;
    double latency_ms;
    double bitrate_kbps;
    double ai_stability_loss;
    int context_width;
    int context_height;
    int roi_tile_width;
    int roi_tile_height;
    int roi_quality;
    int context_quality;
    int tile_cols;
    int tile_rows;
    int cell_width;
    int cell_height;
    bool reencoded;
    std::string controller_state;
    RoiResult roi_result;
};

std::string metadata_to_json(const MetadataPacketInput& input);