#include "rate_controller.hpp"

#include <algorithm>

RateController::RateController(const EngineConfig& config)
    : config_(config),
      roi_quality_(85),
      context_quality_(35),
      detector_interval_(config.detector_interval) {
}

RateControllerOutput RateController::update(const RateControllerInput& input) {
    std::string state = "normal";
    bool overloaded = false;
    bool bitrate_high = input.previous_bitrate_kbps > static_cast<double>(config_.target_bitrate_kbps);
    bool latency_high = input.latency_ms > config_.latency_budget_ms;
    bool queue_high = input.queue_depth >= 2;
    bool drops_high = input.dropped_frames > 0;

    if (latency_high || queue_high || drops_high) {
        overloaded = true;
        state = "overload";
    } else if (bitrate_high) {
        state = "rate_limited";
    }

    if (overloaded) {
        context_quality_ -= 5;
        roi_quality_ -= 2;
        detector_interval_ += 1;
    } else if (bitrate_high) {
        context_quality_ -= 4;

        if (input.roi_area_ratio < 0.08) {
            roi_quality_ -= 1;
        }
    } else {
        if (input.roi_area_ratio > 0.12) {
            roi_quality_ += 2;
        }

        context_quality_ += 1;

        if (detector_interval_ > config_.detector_interval) {
            detector_interval_ -= 1;
        }
    }

    roi_quality_ = std::clamp(roi_quality_, config_.roi_quality_min, config_.roi_quality_max);
    context_quality_ = std::clamp(context_quality_, config_.context_quality_min, config_.context_quality_max);
    detector_interval_ = std::clamp(detector_interval_, 1, 30);

    RateControllerOutput output;
    output.roi_quality = roi_quality_;
    output.context_quality = context_quality_;
    output.detector_interval = detector_interval_;
    output.reencode_allowed = !overloaded && input.latency_ms < 0.7 * config_.latency_budget_ms;
    output.controller_state = state;
    return output;
}