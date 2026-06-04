#include "rate_controller.hpp"

#include <algorithm>

RateController::RateController(const EngineConfig& config)
    : config_(config),
      last_roi_quality_(85),
      last_context_quality_(40),
      last_detector_interval_(config.detector_interval),
      last_context_width_(320),
      last_roi_cell_size_(160),
      last_max_rois_(5) {
}

int RateController::clamp_int(int value, int low, int high) const {
    return std::max(low, std::min(high, value));
}

RateControllerOutput RateController::update(const RateControllerInput& input) {
    RateControllerOutput output;

    bool latency_pressure = input.latency_ms > config_.latency_budget_ms;
    double target_fps = 20.0;
    bool fps_pressure = input.estimated_fps > 0.1 && input.estimated_fps < target_fps * 0.75;
    bool queue_pressure = input.queue_depth >= 2;
    bool dropped_pressure = input.dropped_frames > 0;
    bool bitrate_pressure = input.previous_bitrate_kbps > config_.target_bitrate_kbps * 1.20;
    bool ai_pressure = input.previous_ai_stability_loss > config_.confidence_loss_threshold;
    bool ai_warning = input.previous_ai_stability_loss > 0.60 * config_.confidence_loss_threshold;
    bool large_roi = input.roi_area_ratio > 0.25;

    output.roi_quality = last_roi_quality_;
    output.context_quality = last_context_quality_;
    output.detector_interval = last_detector_interval_;
    output.context_width = last_context_width_;
    output.roi_cell_size = last_roi_cell_size_;
    output.max_rois = last_max_rois_;
    output.reencode_allowed = true;
    output.controller_state = "balanced";
    output.controller_mode = "balanced";
    output.controller_reason = "nominal latency, bitrate, and AI stability";
    output.controller_action = "maintain balanced semantic packet settings";

    if (ai_pressure && !(latency_pressure || fps_pressure || queue_pressure)) {
        output.controller_state = "ai_repair";
        output.controller_mode = "quality";
        output.controller_reason = "AI confidence loss exceeded threshold while latency budget remained available";
        output.controller_action = "increase ROI quality, reduce detector interval, allow re-encode";

        output.roi_quality = clamp_int(last_roi_quality_ + 8, config_.roi_quality_min, config_.roi_quality_max);
        output.context_quality = clamp_int(last_context_quality_ + 2, config_.context_quality_min, config_.context_quality_max);
        output.context_width = clamp_int(last_context_width_, 240, 480);
        output.detector_interval = clamp_int(last_detector_interval_ - 1, 1, config_.detector_interval);
        output.roi_cell_size = clamp_int(last_roi_cell_size_ + 32, 96, 224);
        output.max_rois = 5;
        output.reencode_allowed = true;
    } else if (ai_pressure && (latency_pressure || fps_pressure || queue_pressure)) {
        output.controller_state = "overload_ai_protect";
        output.controller_mode = "realtime_protect";
        output.controller_reason = "system is overloaded but AI confidence loss is high";
        output.controller_action = "protect ROI quality while reducing context and disabling re-encode";

        output.roi_quality = clamp_int(last_roi_quality_ + 4, 60, config_.roi_quality_max);
        output.context_quality = clamp_int(last_context_quality_ - 6, config_.context_quality_min, 35);
        output.context_width = clamp_int(last_context_width_ - 80, 160, 320);
        output.detector_interval = clamp_int(last_detector_interval_ + 5, config_.detector_interval, 45);
        output.roi_cell_size = clamp_int(last_roi_cell_size_, 96, 192);
        output.max_rois = 5;
        output.reencode_allowed = false;
    } else if (latency_pressure || fps_pressure || queue_pressure || dropped_pressure) {
        output.controller_state = "overload_low_fps";
        output.controller_mode = "realtime";
        output.controller_reason = "latency, FPS, queue depth, or dropped-frame pressure detected";
        output.controller_action = "reduce context resolution and quality, increase detector interval, disable re-encode";

        output.roi_quality = clamp_int(last_roi_quality_ - 5, 55, 78);
        output.context_quality = clamp_int(last_context_quality_ - 8, config_.context_quality_min, 32);
        output.context_width = clamp_int(last_context_width_ - 80, 160, 320);
        output.detector_interval = clamp_int(last_detector_interval_ + 5, config_.detector_interval, 45);
        output.roi_cell_size = clamp_int(last_roi_cell_size_ - 32, 96, 160);
        output.max_rois = large_roi ? 3 : 5;
        output.reencode_allowed = false;
    } else if (bitrate_pressure) {
        output.controller_state = "rate_limited";
        output.controller_mode = "balanced_rate";
        output.controller_reason = "semantic packet bitrate exceeded target";
        output.controller_action = "slightly reduce context quality and context resolution while preserving ROI quality";

        output.roi_quality = clamp_int(last_roi_quality_, 65, config_.roi_quality_max);
        output.context_quality = clamp_int(last_context_quality_ - 5, config_.context_quality_min, config_.context_quality_max);
        output.context_width = clamp_int(last_context_width_ - 40, 200, 360);
        output.detector_interval = clamp_int(last_detector_interval_, 1, 30);
        output.roi_cell_size = clamp_int(last_roi_cell_size_, 96, 192);
        output.max_rois = 5;
        output.reencode_allowed = true;
    } else if (ai_warning) {
        output.controller_state = "ai_watch";
        output.controller_mode = "quality_watch";
        output.controller_reason = "AI confidence loss is rising but has not crossed threshold";
        output.controller_action = "slightly raise ROI quality and monitor detector confidence";

        output.roi_quality = clamp_int(last_roi_quality_ + 3, config_.roi_quality_min, config_.roi_quality_max);
        output.context_quality = clamp_int(last_context_quality_, config_.context_quality_min, config_.context_quality_max);
        output.context_width = clamp_int(last_context_width_, 240, 400);
        output.detector_interval = clamp_int(last_detector_interval_ - 1, 1, config_.detector_interval);
        output.roi_cell_size = clamp_int(last_roi_cell_size_, 128, 192);
        output.max_rois = 5;
        output.reencode_allowed = true;
    } else {
        output.controller_state = "balanced";
        output.controller_mode = "balanced";
        output.controller_reason = "system operating within latency, bitrate, and AI-stability targets";
        output.controller_action = "use balanced context and ROI quality";

        output.roi_quality = clamp_int(last_roi_quality_ + (last_roi_quality_ < 78 ? 2 : -1), 68, 84);
        output.context_quality = clamp_int(last_context_quality_ + (last_context_quality_ < 38 ? 2 : -1), 28, 45);
        output.context_width = clamp_int(last_context_width_ + (last_context_width_ < 320 ? 40 : -20), 240, 360);
        output.detector_interval = clamp_int(config_.detector_interval, 1, 20);
        output.roi_cell_size = clamp_int(last_roi_cell_size_ + (last_roi_cell_size_ < 160 ? 32 : 0), 128, 192);
        output.max_rois = 5;
        output.reencode_allowed = true;
    }

    last_roi_quality_ = output.roi_quality;
    last_context_quality_ = output.context_quality;
    last_detector_interval_ = output.detector_interval;
    last_context_width_ = output.context_width;
    last_roi_cell_size_ = output.roi_cell_size;
    last_max_rois_ = output.max_rois;

    return output;
}