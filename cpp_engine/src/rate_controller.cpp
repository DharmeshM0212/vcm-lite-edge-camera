#include "rate_controller.hpp"

#include <algorithm>
#include <cstdint>

RateController::RateController(const EngineConfig& config)
    : config_(config),
      initialized_(false),
      previous_dropped_frames_(0) {
}

RateControllerOutput RateController::balanced_output(const RateControllerInput& input) const {
    RateControllerOutput output;

    output.roi_quality = 72;
    output.context_quality = 32;
    output.detector_interval = 24;
    output.context_width = 320;
    output.roi_cell_size = 160;
    output.max_rois = 5;
    output.reencode_allowed = true;
    output.controller_state = "balanced";
    output.controller_mode = "quality_realtime";
    output.controller_reason = "fps, latency, queue, and AI stability are within Uno Q operating range";
    output.controller_action = "preserve ROI quality, keep moderate context quality, allow AI repair";

    return output;
}

RateControllerOutput RateController::sparse_idle_output(const RateControllerInput& input) const {
    RateControllerOutput output;

    output.roi_quality = 64;
    output.context_quality = 22;
    output.detector_interval = 36;
    output.context_width = 240;
    output.roi_cell_size = 160;
    output.max_rois = 4;
    output.reencode_allowed = false;
    output.controller_state = "sparse_idle";
    output.controller_mode = "event_watch";
    output.controller_reason = "low ROI activity and stable AI loss indicate mostly background scene";
    output.controller_action = "reduce background cost while keeping periodic detector checks for rare vehicle events";

    return output;
}

RateControllerOutput RateController::realtime_protect_output(const RateControllerInput& input) const {
    RateControllerOutput output;

    output.roi_quality = 65;
    output.context_quality = 24;
    output.detector_interval = 36;
    output.context_width = 240;
    output.roi_cell_size = 192;
    output.max_rois = 5;
    output.reencode_allowed = false;
    output.controller_state = "realtime_protect";
    output.controller_mode = "roi_protect";
    output.controller_reason = "moderate latency or FPS pressure detected, but system is not overloaded";
    output.controller_action = "preserve vehicle ROI quality, reduce context quality, use scheduled validation";

    return output;
}

RateControllerOutput RateController::dense_roi_output(const RateControllerInput& input) const {
    RateControllerOutput output;

    output.roi_quality = 62;
    output.context_quality = 22;
    output.detector_interval = 42;
    output.context_width = 240;
    output.roi_cell_size = 192;
    output.max_rois = 5;
    output.reencode_allowed = false;
    output.controller_state = "dense_roi";
    output.controller_mode = "traffic_dense";
    output.controller_reason = "large vehicle ROI area or high semantic packet bitrate detected";
    output.controller_action = "cap ROI count, reduce context cost, preserve vehicle regions";

    return output;
}

RateControllerOutput RateController::overload_output(const RateControllerInput& input) const {
    RateControllerOutput output;

    output.roi_quality = 55;
    output.context_quality = 20;
    output.detector_interval = 60;
    output.context_width = 160;
    output.roi_cell_size = 192;
    output.max_rois = 5;
    output.reencode_allowed = false;
    output.controller_state = "overload_low_fps";
    output.controller_mode = "realtime";
    output.controller_reason = "severe FPS, latency, queue, or recent dropped-frame pressure detected";
    output.controller_action = "reduce context resolution and quality, increase detector interval, disable re-encode";

    return output;
}

RateControllerOutput RateController::update(const RateControllerInput& input) {
    std::uint32_t recent_drops = 0;

    if (initialized_) {
        if (input.dropped_frames >= previous_dropped_frames_) {
            recent_drops = input.dropped_frames - previous_dropped_frames_;
        }
    } else {
        initialized_ = true;
    }

    previous_dropped_frames_ = input.dropped_frames;

    bool severe_fps_pressure = input.estimated_fps > 0.1 && input.estimated_fps < 4.5;
    bool moderate_fps_pressure = input.estimated_fps > 0.1 && input.estimated_fps < 7.0;

    bool severe_latency_pressure = input.latency_ms > 350.0;
    bool moderate_latency_pressure = input.latency_ms > 180.0;

    bool queue_pressure = input.queue_depth >= 2;
    bool drop_pressure = recent_drops >= 3;

    bool ai_pressure = input.previous_ai_stability_loss > 0.10;
    bool ai_moderate = input.previous_ai_stability_loss > 0.07;

    bool dense_roi = input.roi_area_ratio > 0.28 || input.previous_bitrate_kbps > 650.0;
    bool sparse_scene = input.roi_area_ratio < 0.035 && input.previous_ai_stability_loss < 0.06;

    if (severe_fps_pressure || severe_latency_pressure || queue_pressure || drop_pressure || ai_pressure) {
        return overload_output(input);
    }

    if (dense_roi) {
        return dense_roi_output(input);
    }

    if (moderate_fps_pressure || moderate_latency_pressure || ai_moderate) {
        return realtime_protect_output(input);
    }

    if (sparse_scene) {
        return sparse_idle_output(input);
    }

    return balanced_output(input);
}