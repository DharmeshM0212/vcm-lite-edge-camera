#include "frame_queue.hpp"

FrameQueue::FrameQueue(std::size_t capacity)
    : capacity_(capacity), dropped_count_(0) {
}

void FrameQueue::push(Frame frame) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (queue_.size() >= capacity_) {
        queue_.pop_front();
        dropped_count_++;
    }

    queue_.push_back(std::move(frame));
}

std::optional<Frame> FrameQueue::pop() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (queue_.empty()) {
        return std::nullopt;
    }

    Frame frame = std::move(queue_.front());
    queue_.pop_front();
    return frame;
}

std::size_t FrameQueue::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
}

std::uint64_t FrameQueue::dropped_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return dropped_count_;
}