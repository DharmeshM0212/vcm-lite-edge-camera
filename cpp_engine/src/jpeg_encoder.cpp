#include "jpeg_encoder.hpp"

#include "opencv_bridge.hpp"

#include <opencv2/opencv.hpp>
#include <stdexcept>

EncodedImage encode_jpeg(const Frame& frame, int quality) {
    cv::Mat mat = frame_to_mat(frame);

    std::vector<int> params;
    params.push_back(cv::IMWRITE_JPEG_QUALITY);
    params.push_back(quality);

    EncodedImage encoded;

    if (!cv::imencode(".jpg", mat, encoded.bytes, params)) {
        throw std::runtime_error("jpeg_encode_failed");
    }

    return encoded;
}

Frame decode_jpeg(const EncodedImage& encoded, std::uint64_t frame_id) {
    if (encoded.bytes.empty()) {
        throw std::runtime_error("jpeg_decode_empty_input");
    }

    cv::Mat buffer(1, static_cast<int>(encoded.bytes.size()), CV_8UC1, const_cast<std::uint8_t*>(encoded.bytes.data()));
    cv::Mat decoded = cv::imdecode(buffer, cv::IMREAD_COLOR);

    if (decoded.empty()) {
        throw std::runtime_error("jpeg_decode_failed");
    }

    return mat_to_frame(decoded, frame_id);
}

double estimate_frame_bitrate_kbps(std::size_t total_bytes, double fps) {
    return static_cast<double>(total_bytes) * 8.0 * fps / 1000.0;
}