#include "output_writer.hpp"

#include "opencv_bridge.hpp"

#include <opencv2/opencv.hpp>

bool write_frame_image(const Frame& frame, const std::string& path) {
    cv::Mat mat = frame_to_mat(frame);

    if (mat.empty()) {
        return false;
    }

    return cv::imwrite(path, mat);
}