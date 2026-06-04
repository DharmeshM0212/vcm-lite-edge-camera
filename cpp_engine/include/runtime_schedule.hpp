#pragma once

#include <cstdint>

struct RuntimeSchedule {
    int warmup_detector_frames;
    int compressed_validation_interval;
    int semantic_packet_interval;
    int detection_event_interval;
    int minimum_detector_interval;
    int overload_detector_interval;
};

RuntimeSchedule default_runtime_schedule();

bool should_run_compressed_validation(
    std::uint64_t frame_id,
    bool detector_ran_this_frame,
    double previous_ai_stability_loss,
    double confidence_loss_threshold,
    const RuntimeSchedule& schedule
);

bool should_write_semantic_packet(
    std::uint64_t frame_id,
    bool detector_ran_this_frame,
    double ai_stability_loss,
    double confidence_loss_threshold,
    const RuntimeSchedule& schedule
);

bool should_write_detection_event(
    std::uint64_t frame_id,
    bool detector_ran_this_frame,
    bool has_detected_objects,
    const RuntimeSchedule& schedule
);