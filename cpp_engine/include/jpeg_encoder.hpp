#pragma once

#include "frame.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

struct EncodedImage {
    std::vector<std::uint8_t> bytes;
};

EncodedImage encode_jpeg(const Frame& frame, int quality);
Frame decode_jpeg(const EncodedImage& encoded, std::uint64_t frame_id);
double estimate_frame_bitrate_kbps(std::size_t total_bytes, double fps);