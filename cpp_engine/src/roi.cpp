#include "roi.hpp"

#include <algorithm>

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