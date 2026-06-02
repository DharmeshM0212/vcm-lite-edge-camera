#include "semantic_reconstruct.hpp"

#include "opencv_bridge.hpp"

#include <algorithm>
#include <opencv2/opencv.hpp>

Frame reconstruct_semantic_frame(const Frame& context_frame, const PackedRoiTile& roi_tile, const RoiResult& roi_result, int output_width, int output_height) {
    cv::Mat context = frame_to_mat(context_frame);
    cv::Mat tile = frame_to_mat(roi_tile.tile);

    cv::Mat reconstructed;
    cv::resize(context, reconstructed, cv::Size(output_width, output_height), 0.0, 0.0, cv::INTER_LINEAR);

    int roi_count = std::min(static_cast<int>(roi_result.boxes.size()), roi_tile.tile_cols * roi_tile.tile_rows);

    for (int i = 0; i < roi_count; i++) {
        const RoiBox& box = roi_result.boxes[static_cast<std::size_t>(i)];

        int tile_col = i % roi_tile.tile_cols;
        int tile_row = i / roi_tile.tile_cols;

        int tile_x = tile_col * roi_tile.cell_width;
        int tile_y = tile_row * roi_tile.cell_height;

        if (tile_x + roi_tile.cell_width > tile.cols || tile_y + roi_tile.cell_height > tile.rows) {
            continue;
        }

        int dst_x = std::clamp(box.x, 0, output_width - 1);
        int dst_y = std::clamp(box.y, 0, output_height - 1);
        int dst_w = std::clamp(box.width, 1, output_width - dst_x);
        int dst_h = std::clamp(box.height, 1, output_height - dst_y);

        cv::Rect tile_rect(tile_x, tile_y, roi_tile.cell_width, roi_tile.cell_height);
        cv::Mat roi_cell = tile(tile_rect);

        cv::Mat resized_roi;
        cv::resize(roi_cell, resized_roi, cv::Size(dst_w, dst_h), 0.0, 0.0, cv::INTER_LINEAR);

        cv::Rect dst_rect(dst_x, dst_y, dst_w, dst_h);
        resized_roi.copyTo(reconstructed(dst_rect));
    }

    return mat_to_frame(reconstructed, context_frame.id);
}