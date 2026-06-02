#include "rate_controller.hpp"

#include <algorithm>

RateController::RateController(const EngineConfig& config)
    : config_(config),
      roi_quality_(85),
      context_quality_(38),
      detector_interval_(config.detector_interval),
      context_width_(320),
      roi_cell_size_(160),
      max_rois_(5),
      previous_dropped_frames_(0) {
}

double RateController::bitrate_error_ratio(double bitrate_kbps) const {
    double target = static_cast<double>(std::max(1, config_.target_bitrate_kbps));
    return (bitrate_kbps - target) / target;
}

bool RateController::is_overloaded(const RateControllerInput& input) const {
    bool latency_high = input.latency_ms > config_.latency_budget_ms;
    bool queue_high = input.queue_depth >= 2;
    bool new_drops = input.dropped_frames > previous_dropped_frames_;
    return latency_high || queue_high || new_drops;
}

bool RateController::is_ai_unstable(const RateControllerInput& input) const {
    return input.previous_ai_stability_loss > config_.confidence_loss_threshold;
}

bool RateController::is_rate_limited(const RateControllerInput& input) const {
    return bitrate_error_ratio(input.previous_bitrate_kbps) > 0.08;
}

RateControllerOutput RateController::update(const RateControllerInput& input) {
    bool overloaded = is_overloaded(input);
    bool ai_unstable = is_ai_unstable(input);
    bool rate_limited = is_rate_limited(input);
    double bitrate_error = bitrate_error_ratio(input.previous_bitrate_kbps);

    std::string state = "normal";

    if (overloaded && ai_unstable) {
        state = "overload_ai_protect";
        context_quality_ -= 8;
        roi_quality_ += 1;
        detector_interval_ += 3;
        context_width_ -= 80;
        roi_cell_size_ -= 16;
        max_rois_ = std::max(2, max_rois_ - 1);
    } else if (overloaded) {
        state = "overload";
        context_quality_ -= 8;
        roi_quality_ -= input.roi_area_ratio < 0.04 ? 2 : 0;
        detector_interval_ += 3;
        context_width_ -= 80;
        roi_cell_size_ -= 24;
        max_rois_ = std::max(1, max_rois_ - 1);
    } else if (ai_unstable && rate_limited) {
        state = "ai_protect_rate_limited";
        roi_quality_ += 5;
        context_quality_ -= 8;
        context_width_ -= 40;
        roi_cell_size_ = std::max(128, roi_cell_size_ - 8);

        if (detector_interval_ > 1) {
            detector_interval_ -= 1;
        }
    } else if (ai_unstable) {
        state = "ai_protect";
        roi_quality_ += 6;
        context_quality_ -= 2;
        roi_cell_size_ += 8;

        if (detector_interval_ > 1) {
            detector_interval_ -= 1;
        }

        if (input.roi_area_ratio > 0.08) {
            max_rois_ = std::min(5, max_rois_ + 1);
        }
    } else if (rate_limited) {
        state = "rate_limited";
        int context_step = bitrate_error > 0.50 ? 8 : 5;
        context_quality_ -= context_step;
        context_width_ -= bitrate_error > 0.50 ? 80 : 40;
        roi_cell_size_ -= bitrate_error > 0.50 ? 16 : 8;

        if (input.roi_area_ratio < 0.03) {
            roi_quality_ -= 1;
        }

        detector_interval_ += bitrate_error > 0.50 ? 2 : 1;
    } else {
        state = "normal";
        context_quality_ += 2;

        if (context_width_ < 320) {
            context_width_ += 20;
        }

        if (roi_cell_size_ < 160) {
            roi_cell_size_ += 4;
        }

        if (input.roi_area_ratio > 0.10) {
            roi_quality_ += 1;
            max_rois_ = std::min(5, max_rois_ + 1);
        } else if (input.roi_area_ratio < 0.02 && input.previous_ai_stability_loss < 0.5 * config_.confidence_loss_threshold) {
            roi_quality_ -= 1;
        }

        if (detector_interval_ > config_.detector_interval) {
            detector_interval_ -= 1;
        }
    }

    if (input.estimated_fps < 18.0) {
        state = state == "normal" ? "low_fps" : state + "_low_fps";
        context_quality_ -= 4;
        context_width_ -= 40;
        roi_cell_size_ -= 8;
        detector_interval_ += 2;
    }

    roi_quality_ = std::clamp(roi_quality_, config_.roi_quality_min, config_.roi_quality_max);
    context_quality_ = std::clamp(context_quality_, config_.context_quality_min, config_.context_quality_max);
    detector_interval_ = std::clamp(detector_interval_, 1, 30);
    context_width_ = std::clamp(context_width_, 160, 320);
    roi_cell_size_ = std::clamp(roi_cell_size_, 96, 192);
    max_rois_ = std::clamp(max_rois_, 1, 5);

    bool latency_safe = input.latency_ms < 0.65 * config_.latency_budget_ms;
    bool queue_safe = input.queue_depth == 0;
    bool drops_safe = input.dropped_frames == previous_dropped_frames_;
    bool reencode_allowed = latency_safe && queue_safe && drops_safe && !overloaded;

    previous_dropped_frames_ = input.dropped_frames;

    RateControllerOutput output;
    output.roi_quality = roi_quality_;
    output.context_quality = context_quality_;
    output.detector_interval = detector_interval_;
    output.context_width = context_width_;
    output.roi_cell_size = roi_cell_size_;
    output.max_rois = max_rois_;
    output.reencode_allowed = reencode_allowed;
    output.controller_state = state;
    return output;
}