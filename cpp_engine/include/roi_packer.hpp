#pragma once

#include "frame.hpp"
#include "roi.hpp"

struct PackedRoiTile {
    Frame tile;
    int tile_cols;
    int tile_rows;
    int cell_width;
    int cell_height;
};

PackedRoiTile pack_roi_tile(const Frame& frame, const RoiResult& roi_result, int cell_width, int cell_height, int max_rois);
Frame make_context_frame(const Frame& frame, int target_width);