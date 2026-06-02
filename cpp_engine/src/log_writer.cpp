#include "log_writer.hpp"

JsonlLogWriter::JsonlLogWriter(const std::string& path)
    : stream_(path, std::ios::out | std::ios::trunc) {
}

bool JsonlLogWriter::is_open() const {
    return stream_.is_open();
}

void JsonlLogWriter::write_line(const std::string& line) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!stream_.is_open()) {
        return;
    }

    stream_ << line << '\n';
    stream_.flush();
}