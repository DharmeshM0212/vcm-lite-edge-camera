#pragma once

#include "frame.hpp"

#include <cstddef>
#include <deque>
#include <mutex>
#include <optional>

class FrameQueue {
public:
    explicit FrameQueue(std::size_t capacity);

    void push(Frame frame);
    std::optional<Frame> pop();
    std::size_t size() const;
    std::uint64_t dropped_count() const;

private:
    std::size_t capacity_;
    mutable std::mutex mutex_;
    std::deque<Frame> queue_;
    std::uint64_t dropped_count_;
};