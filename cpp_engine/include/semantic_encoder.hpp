#pragma once

#include "frame.hpp"
#include "roi.hpp"

struct SemanticEncodeResult {
    int roi_quality;
    int context_quality;
    double estimated_bitrate_kbps;
};

SemanticEncodeResult estimate_semantic_encode(const Frame& frame, const RoiResult& roi_result, int roi_quality, int context_quality);