#pragma once

#include <fstream>
#include <mutex>
#include <string>

class JsonlLogWriter {
public:
    explicit JsonlLogWriter(const std::string& path);

    bool is_open() const;
    void write_line(const std::string& line);

private:
    mutable std::mutex mutex_;
    std::ofstream stream_;
};