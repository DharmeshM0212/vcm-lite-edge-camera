#include "video_source.hpp"

#include "opencv_bridge.hpp"

VideoSource::VideoSource(const std::string& path)
    : capture_(path), next_frame_id_(0) {
}

bool VideoSource::is_opened() const {
    return capture_.isOpened();
}

double VideoSource::fps() const {
    return capture_.get(cv::CAP_PROP_FPS);
}

int VideoSource::width() const {
    return static_cast<int>(capture_.get(cv::CAP_PROP_FRAME_WIDTH));
}

int VideoSource::height() const {
    return static_cast<int>(capture_.get(cv::CAP_PROP_FRAME_HEIGHT));
}

std::optional<Frame> VideoSource::read() {
    cv::Mat mat;

    if (!capture_.read(mat)) {
        return std::nullopt;
    }

    Frame frame = mat_to_frame(mat, next_frame_id_);
    next_frame_id_++;
    return frame;
}