#include "roi_packer.hpp"

#include "opencv_bridge.hpp"

#include <algorithm>
#include <cmath>
#include <opencv2/opencv.hpp>

Frame make_context_frame(const Frame& frame, int target_width) {
    cv::Mat input = frame_to_mat(frame);

    if (target_width <= 0 || frame.width <= 0 || frame.height <= 0) {
        return frame;
    }

    double scale = static_cast<double>(target_width) / static_cast<double>(frame.width);
    int target_height = std::max(1, static_cast<int>(std::round(static_cast<double>(frame.height) * scale)));

    cv::Mat resized;
    cv::resize(input, resized, cv::Size(target_width, target_height), 0.0, 0.0, cv::INTER_AREA);

    return mat_to_frame(resized, frame.id);
}

PackedRoiTile pack_roi_tile(const Frame& frame, const RoiResult& roi_result, int cell_width, int cell_height, int max_rois) {
    int roi_count = std::min(static_cast<int>(roi_result.boxes.size()), max_rois);

    if (roi_count <= 0) {
        cv::Mat empty_tile(cell_height, cell_width, CV_8UC3, cv::Scalar(0, 0, 0));

        PackedRoiTile packed;
        packed.tile = mat_to_frame(empty_tile, frame.id);
        packed.tile_cols = 1;
        packed.tile_rows = 1;
        packed.cell_width = cell_width;
        packed.cell_height = cell_height;
        return packed;
    }

    int tile_cols = static_cast<int>(std::ceil(std::sqrt(static_cast<double>(roi_count))));
    int tile_rows = static_cast<int>(std::ceil(static_cast<double>(roi_count) / static_cast<double>(tile_cols)));

    cv::Mat input = frame_to_mat(frame);
    cv::Mat tile(tile_rows * cell_height, tile_cols * cell_width, input.type(), cv::Scalar(0, 0, 0));

    for (int i = 0; i < roi_count; i++) {
        const RoiBox& box = roi_result.boxes[static_cast<std::size_t>(i)];

        int x = std::clamp(box.x, 0, frame.width - 1);
        int y = std::clamp(box.y, 0, frame.height - 1);
        int w = std::clamp(box.width, 1, frame.width - x);
        int h = std::clamp(box.height, 1, frame.height - y);

        cv::Rect src_rect(x, y, w, h);
        cv::Mat crop = input(src_rect);

        cv::Mat resized;
        cv::resize(crop, resized, cv::Size(cell_width, cell_height), 0.0, 0.0, cv::INTER_LINEAR);

        int row = i / tile_cols;
        int col = i % tile_cols;
        cv::Rect dst_rect(col * cell_width, row * cell_height, cell_width, cell_height);

        resized.copyTo(tile(dst_rect));
    }

    PackedRoiTile packed;
    packed.tile = mat_to_frame(tile, frame.id);
    packed.tile_cols = tile_cols;
    packed.tile_rows = tile_rows;
    packed.cell_width = cell_width;
    packed.cell_height = cell_height;
    return packed;
}