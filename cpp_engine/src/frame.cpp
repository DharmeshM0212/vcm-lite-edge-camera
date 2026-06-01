#include "frame.hpp"

Frame make_empty_frame(std::uint64_t id, int width, int height, int channels) {
    Frame frame;
    frame.id = id;
    frame.width = width;
    frame.height = height;
    frame.channels = channels;
    frame.created_at = std::chrono::steady_clock::now();
    frame.data.resize(static_cast<std::size_t>(width * height * channels));
    return frame;
}