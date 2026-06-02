#pragma once

#include "frame.hpp"
#include "roi.hpp"
#include "roi_packer.hpp"

Frame reconstruct_semantic_frame(const Frame& context_frame, const PackedRoiTile& roi_tile, const RoiResult& roi_result, int output_width, int output_height);