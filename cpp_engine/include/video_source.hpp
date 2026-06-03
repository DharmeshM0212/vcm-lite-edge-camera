#pragma once

#include "frame.hpp"

#include <cstdint>
#include <optional>
#include <string>

class VideoSource {
public:
    explicit VideoSource(const std::string& path);
    ~VideoSource();

    bool is_opened() const;
    double fps() const;
    std::optional<Frame> read();

private:
    enum class SourceMode {
        VideoFile,
        LiveImageDirectory,
        FrameSocket
    };

    SourceMode mode_;
    std::string path_;
    std::string live_image_path_;
    bool opened_;
    double fps_;
    std::uint64_t frame_id_;
    std::string last_signature_;
    std::uintptr_t socket_handle_;

    std::optional<Frame> read_video_file();
    std::optional<Frame> read_live_image();
    std::optional<Frame> read_socket_frame();

    bool connect_socket_source(const std::string& uri);
    void close_socket();
    bool recv_all(unsigned char* data, std::size_t size);
    std::string file_signature(const std::string& path) const;
};