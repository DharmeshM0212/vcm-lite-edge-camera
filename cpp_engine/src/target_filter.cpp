#include "target_filter.hpp"

#include <algorithm>
#include <cctype>
#include <string>

static std::string normalize_label(const std::string& label) {
    std::string output;
    output.reserve(label.size());

    for (char c : label) {
        if (c == '_' || c == '-') {
            output.push_back(' ');
        } else {
            output.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }
    }

    while (!output.empty() && output.front() == ' ') {
        output.erase(output.begin());
    }

    while (!output.empty() && output.back() == ' ') {
        output.pop_back();
    }

    return output;
}

static bool labels_match(const std::string& a, const std::string& b) {
    return normalize_label(a) == normalize_label(b);
}

static bool find_threshold(
    const std::string& label,
    const TargetFilterPolicy& policy,
    double& threshold
) {
    for (const auto& rule : policy.rules) {
        if (labels_match(label, rule.label)) {
            threshold = rule.min_confidence;
            return true;
        }
    }

    return false;
}

static double mean_confidence_of_objects(const std::vector<DetectedObject>& objects) {
    if (objects.empty()) {
        return 0.0;
    }

    double sum = 0.0;

    for (const auto& object : objects) {
        sum += object.confidence;
    }

    return sum / static_cast<double>(objects.size());
}

TargetFilterPolicy target_filter_roi_policy() {
    TargetFilterPolicy policy;

    policy.rules = {
        {"car", 0.20},
        {"van", 0.20},
        {"bus", 0.20},
        {"others", 0.20}
    };

    policy.fallback_min_confidence = 0.25;
    policy.keep_unknown_classes = false;

    return policy;
}

TargetFilterPolicy target_filter_event_policy() {
    TargetFilterPolicy policy;

    policy.rules = {
        {"car", 0.25},
        {"van", 0.25},
        {"bus", 0.25},
        {"others", 0.25}
    };

    policy.fallback_min_confidence = 0.25;
    policy.keep_unknown_classes = false;

    return policy;
}

DetectorResult filter_task_relevant_objects(
    const DetectorResult& input,
    const TargetFilterPolicy& policy
) {
    DetectorResult output = input;
    output.objects.clear();

    for (const auto& object : input.objects) {
        double threshold = policy.fallback_min_confidence;
        bool known_target = find_threshold(object.label, policy, threshold);

        if (!known_target && !policy.keep_unknown_classes) {
            continue;
        }

        if (object.confidence >= threshold) {
            output.objects.push_back(object);
        }
    }

    output.mean_confidence = mean_confidence_of_objects(output.objects);

    return output;
}