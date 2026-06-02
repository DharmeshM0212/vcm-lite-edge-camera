#include "opencv_bridge.hpp"

#include <cstring>
#include <stdexcept>

Frame mat_to_frame(const cv::Mat& mat, std::uint64_t id) {
    if (mat.empty()) {
        throw std::runtime_error("empty cv::Mat");
    }

    cv::Mat contiguous;

    if (mat.isContinuous()) {
        contiguous = mat;
    } else {
        contiguous = mat.clone();
    }

    Frame frame;
    frame.id = id;
    frame.width = contiguous.cols;
    frame.height = contiguous.rows;
    frame.channels = contiguous.channels();
    frame.created_at = std::chrono::steady_clock::now();

    std::size_t byte_count = contiguous.total() * contiguous.elemSize();
    frame.data.resize(byte_count);
    std::memcpy(frame.data.data(), contiguous.data, byte_count);

    return frame;
}

cv::Mat frame_to_mat(const Frame& frame) {
    int type;

    if (frame.channels == 1) {
        type = CV_8UC1;
    } else if (frame.channels == 3) {
        type = CV_8UC3;
    } else if (frame.channels == 4) {
        type = CV_8UC4;
    } else {
        throw std::runtime_error("unsupported channel count");
    }

    cv::Mat mat(frame.height, frame.width, type);
    std::size_t byte_count = frame.data.size();

    if (byte_count != mat.total() * mat.elemSize()) {
        throw std::runtime_error("frame buffer size mismatch");
    }

    std::memcpy(mat.data, frame.data.data(), byte_count);
    return mat;
}