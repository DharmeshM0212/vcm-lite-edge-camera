#pragma once

#include <chrono>
#include <cstdint>
#include <vector>

struct Frame {
    std::uint64_t id;
    int width;
    int height;
    int channels;
    std::chrono::steady_clock::time_point created_at;
    std::vector<std::uint8_t> data;
};

Frame make_empty_frame(std::uint64_t id, int width, int height, int channels);