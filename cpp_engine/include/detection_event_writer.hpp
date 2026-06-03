#pragma once

#include "ai_detector.hpp"
#include "frame.hpp"

#include <cstdint>
#include <string>

bool write_detection_event_snapshot(
    const Frame& reference_frame,
    const Frame& reconstructed_frame,
    const DetectorResult& detector_result,
    std::uint64_t frame_id,
    double reference_ai_confidence,
    double compressed_ai_confidence,
    double detector_confidence_loss,
    double ai_stability_loss,
    const std::string& output_dir
);