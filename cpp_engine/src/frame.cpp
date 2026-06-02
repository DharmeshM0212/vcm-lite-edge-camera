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

Frame make_synthetic_frame(std::uint64_t id, int width, int height, int channels) {
    Frame frame = make_empty_frame(id, width, height, channels);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            std::size_t base = static_cast<std::size_t>((y * width + x) * channels);
            frame.data[base] = static_cast<std::uint8_t>((x + id) % 256);

            if (channels > 1) {
                frame.data[base + 1] = static_cast<std::uint8_t>((y + id) % 256);
            }

            if (channels > 2) {
                frame.data[base + 2] = static_cast<std::uint8_t>((x + y + id) % 256);
            }
        }
    }

    return frame;
}