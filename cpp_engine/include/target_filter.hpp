#pragma once

#include "ai_detector.hpp"

#include <string>
#include <vector>

struct TargetClassRule {
    std::string label;
    double min_confidence;
};

struct TargetFilterPolicy {
    std::vector<TargetClassRule> rules;
    double fallback_min_confidence;
    bool keep_unknown_classes;
};

TargetFilterPolicy target_filter_roi_policy();
TargetFilterPolicy target_filter_event_policy();

DetectorResult filter_task_relevant_objects(
    const DetectorResult& input,
    const TargetFilterPolicy& policy
);