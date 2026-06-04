#pragma once

#include "ai_detector.hpp"
#include "jpeg_encoder.hpp"
#include "roi.hpp"

#include <cstdint>
#include <string>

struct SemanticPacketInput {
    std::uint64_t frame_id;
    double timestamp_ms;
    std::uint32_t frame_width;
    std::uint32_t frame_height;
    std::uint32_t context_width;
    std::uint32_t context_height;
    std::uint32_t roi_tile_width;
    std::uint32_t roi_tile_height;
    std::uint32_t roi_quality;
    std::uint32_t context_quality;
    std::uint32_t detector_interval;
    std::string controller_state;
    double reference_ai_confidence;
    double compressed_ai_confidence;
    double detector_confidence_loss;
    double ai_stability_loss;
    RoiResult roi_result;
    DetectorResult detector_result;
    EncodedImage context_jpeg;
    EncodedImage roi_tile_jpeg;
};

std::string semantic_packet_metadata_json(const SemanticPacketInput& input);
bool write_semantic_packet(const SemanticPacketInput& input, const std::string& output_dir);