#include "roi.hpp"

#include <algorithm>
#include <cmath>

RoiResult detect_synthetic_roi(const Frame& frame) {
    RoiResult result;

    int box_width = frame.width / 5;
    int box_height = frame.height / 4;
    int usable_width = std::max(1, frame.width - box_width);
    int usable_height = std::max(1, frame.height - box_height);

    RoiBox box;
    box.x = static_cast<int>((frame.id * 17) % static_cast<std::uint64_t>(usable_width));
    box.y = static_cast<int>((frame.id * 9) % static_cast<std::uint64_t>(usable_height));
    box.width = box_width;
    box.height = box_height;
    box.confidence = 0.90;

    result.boxes.push_back(box);
    return result;
}

double roi_area_ratio(const Frame& frame, const RoiResult& roi_result) {
    double roi_area = 0.0;

    for (const auto& box : roi_result.boxes) {
        roi_area += static_cast<double>(box.width * box.height);
    }

    double frame_area = static_cast<double>(frame.width * frame.height);
    return frame_area > 0.0 ? roi_area / frame_area : 0.0;
}

RoiBox clamp_roi_box(const RoiBox& box, int frame_width, int frame_height) {
    RoiBox clamped;
    clamped.x = std::clamp(box.x, 0, std::max(0, frame_width - 1));
    clamped.y = std::clamp(box.y, 0, std::max(0, frame_height - 1));
    clamped.width = std::clamp(box.width, 1, std::max(1, frame_width - clamped.x));
    clamped.height = std::clamp(box.height, 1, std::max(1, frame_height - clamped.y));
    clamped.confidence = std::clamp(box.confidence, 0.0, 1.0);
    return clamped;
}

double roi_iou(const RoiBox& a, const RoiBox& b) {
    int ax2 = a.x + a.width;
    int ay2 = a.y + a.height;
    int bx2 = b.x + b.width;
    int by2 = b.y + b.height;

    int ix1 = std::max(a.x, b.x);
    int iy1 = std::max(a.y, b.y);
    int ix2 = std::min(ax2, bx2);
    int iy2 = std::min(ay2, by2);

    int iw = std::max(0, ix2 - ix1);
    int ih = std::max(0, iy2 - iy1);

    double intersection = static_cast<double>(iw * ih);
    double union_area = static_cast<double>(a.width * a.height + b.width * b.height) - intersection;

    if (union_area <= 0.0) {
        return 0.0;
    }

    return intersection / union_area;
}

RoiResult merge_overlapping_rois(const RoiResult& input, double iou_threshold, int frame_width, int frame_height) {
    RoiResult result;

    for (const auto& raw_box : input.boxes) {
        RoiBox box = clamp_roi_box(raw_box, frame_width, frame_height);
        bool merged = false;

        for (auto& existing : result.boxes) {
            if (roi_iou(existing, box) >= iou_threshold) {
                int x1 = std::min(existing.x, box.x);
                int y1 = std::min(existing.y, box.y);
                int x2 = std::max(existing.x + existing.width, box.x + box.width);
                int y2 = std::max(existing.y + existing.height, box.y + box.height);

                existing.x = x1;
                existing.y = y1;
                existing.width = x2 - x1;
                existing.height = y2 - y1;
                existing.confidence = std::max(existing.confidence, box.confidence);
                existing = clamp_roi_box(existing, frame_width, frame_height);
                merged = true;
                break;
            }
        }

        if (!merged) {
            result.boxes.push_back(box);
        }
    }

    std::sort(result.boxes.begin(), result.boxes.end(), [](const RoiBox& a, const RoiBox& b) {
        return a.width * a.height > b.width * b.height;
    });

    return result;
}

RoiResult limit_rois(const RoiResult& input, int max_rois) {
    RoiResult result = input;

    std::sort(result.boxes.begin(), result.boxes.end(), [](const RoiBox& a, const RoiBox& b) {
        return a.confidence * static_cast<double>(a.width * a.height) > b.confidence * static_cast<double>(b.width * b.height);
    });

    if (static_cast<int>(result.boxes.size()) > max_rois) {
        result.boxes.resize(static_cast<std::size_t>(max_rois));
    }

    return result;
}