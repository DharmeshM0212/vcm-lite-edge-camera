#include "runtime_schedule.hpp"

RuntimeSchedule default_runtime_schedule() {
    RuntimeSchedule schedule;

    schedule.warmup_detector_frames = 3;
    schedule.compressed_validation_interval = 45;
    schedule.semantic_packet_interval = 30;
    schedule.detection_event_interval = 10;
    schedule.minimum_detector_interval = 15;
    schedule.overload_detector_interval = 60;
    schedule.object_roi_reuse_frames = 8;

    return schedule;
}

bool should_run_compressed_validation(
    std::uint64_t frame_id,
    bool detector_ran_this_frame,
    bool has_reference_objects,
    double previous_ai_stability_loss,
    double confidence_loss_threshold,
    const RuntimeSchedule& schedule
) {
    if (!detector_ran_this_frame || !has_reference_objects) {
        return false;
    }

    if (frame_id < static_cast<std::uint64_t>(schedule.warmup_detector_frames)) {
        return true;
    }

    if (previous_ai_stability_loss > 0.75 * confidence_loss_threshold) {
        return true;
    }

    return frame_id % static_cast<std::uint64_t>(schedule.compressed_validation_interval) == 0;
}

bool should_write_semantic_packet(
    std::uint64_t frame_id,
    bool detector_ran_this_frame,
    double ai_stability_loss,
    double confidence_loss_threshold,
    const RuntimeSchedule& schedule
) {
    if (frame_id % static_cast<std::uint64_t>(schedule.semantic_packet_interval) == 0) {
        return true;
    }

    if (detector_ran_this_frame && ai_stability_loss > 0.60 * confidence_loss_threshold) {
        return true;
    }

    return false;
}

bool should_write_detection_event(
    std::uint64_t frame_id,
    bool detector_ran_this_frame,
    bool has_detected_objects,
    const RuntimeSchedule& schedule
) {
    if (!detector_ran_this_frame || !has_detected_objects) {
        return false;
    }

    return frame_id % static_cast<std::uint64_t>(schedule.detection_event_interval) == 0;
}