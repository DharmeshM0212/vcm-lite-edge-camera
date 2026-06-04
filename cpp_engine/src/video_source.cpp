#include "video_source.hpp"

#include "opencv_bridge.hpp"

#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <opencv2/opencv.hpp>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

class VideoCaptureHolder {
public:
    cv::VideoCapture capture;
};

static VideoCaptureHolder& holder() {
    static VideoCaptureHolder instance;
    return instance;
}

static std::string read_text_file(const fs::path& path) {
    std::ifstream file(path);

    if (!file.is_open()) {
        return "";
    }

    std::string value;
    std::getline(file, value);
    return value;
}

static bool starts_with(const std::string& text, const std::string& prefix) {
    return text.rfind(prefix, 0) == 0;
}

static std::pair<std::string, int> parse_tcp_uri(const std::string& uri) {
    std::string value = uri.substr(std::strlen("tcp://"));
    std::size_t colon = value.find(':');

    if (colon == std::string::npos) {
        return {value, 5001};
    }

    std::string host = value.substr(0, colon);
    int port = std::stoi(value.substr(colon + 1));

    return {host, port};
}

static std::uint32_t read_u32_be(const unsigned char* data) {
    return
        (static_cast<std::uint32_t>(data[0]) << 24) |
        (static_cast<std::uint32_t>(data[1]) << 16) |
        (static_cast<std::uint32_t>(data[2]) << 8) |
        static_cast<std::uint32_t>(data[3]);
}

VideoSource::VideoSource(const std::string& path)
    : mode_(SourceMode::VideoFile),
      path_(path),
      opened_(false),
      fps_(30.0),
      frame_id_(0),
      socket_handle_(0) {
    if (starts_with(path, "tcp://")) {
        mode_ = SourceMode::FrameSocket;
        opened_ = connect_socket_source(path);
        fps_ = 30.0;
        return;
    }

    fs::path input_path(path);

    if (fs::exists(input_path) && fs::is_directory(input_path)) {
        mode_ = SourceMode::LiveImageDirectory;
        live_image_path_ = (input_path / "webrtc_frames" / "latest.txt").string();
        opened_ = true;
        fps_ = 30.0;
        return;
    }

    mode_ = SourceMode::VideoFile;
    holder().capture.open(path);

    if (!holder().capture.isOpened()) {
        opened_ = false;
        return;
    }

    opened_ = true;
    fps_ = holder().capture.get(cv::CAP_PROP_FPS);

    if (fps_ <= 1.0 || fps_ > 240.0) {
        fps_ = 30.0;
    }
}

VideoSource::~VideoSource() {
    close_socket();
}

bool VideoSource::is_opened() const {
    return opened_;
}

double VideoSource::fps() const {
    return fps_;
}

std::optional<Frame> VideoSource::read() {
    if (!opened_) {
        return std::nullopt;
    }

    if (mode_ == SourceMode::FrameSocket) {
        return read_socket_frame();
    }

    if (mode_ == SourceMode::LiveImageDirectory) {
        return read_live_image();
    }

    return read_video_file();
}

std::optional<Frame> VideoSource::read_video_file() {
    cv::Mat frame;

    if (!holder().capture.read(frame)) {
        return std::nullopt;
    }

    return mat_to_frame(frame, frame_id_++);
}

std::string VideoSource::file_signature(const std::string& path) const {
    try {
        fs::path file_path(path);

        if (!fs::exists(file_path)) {
            return "";
        }

        auto size = fs::file_size(file_path);
        auto time = fs::last_write_time(file_path).time_since_epoch().count();

        return std::to_string(size) + ":" + std::to_string(time);
    } catch (...) {
        return "";
    }
}

std::optional<Frame> VideoSource::read_live_image() {
    fs::path latest_txt(live_image_path_);

    for (int attempt = 0; attempt < 120; attempt++) {
        std::string signature = file_signature(live_image_path_);

        if (!signature.empty() && signature != last_signature_) {
            std::string filename = read_text_file(latest_txt);

            if (!filename.empty()) {
                fs::path frame_path = latest_txt.parent_path() / filename;

                cv::Mat image = cv::imread(frame_path.string(), cv::IMREAD_COLOR);

                if (!image.empty()) {
                    last_signature_ = signature;
                    return mat_to_frame(image, frame_id_++);
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    return std::nullopt;
}

bool VideoSource::connect_socket_source(const std::string& uri) {
    auto [host, port] = parse_tcp_uri(uri);

#ifdef _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        std::cerr << "socket_wsa_startup_failed" << std::endl;
        return false;
    }
#endif

    for (int attempt = 0; attempt < 100; attempt++) {
#ifdef _WIN32
        SOCKET sock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock == INVALID_SOCKET) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
#else
        int sock = ::socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
#endif

        sockaddr_in address;
        std::memset(&address, 0, sizeof(address));
        address.sin_family = AF_INET;
        address.sin_port = htons(static_cast<unsigned short>(port));

        if (inet_pton(AF_INET, host.c_str(), &address.sin_addr) <= 0) {
#ifdef _WIN32
            closesocket(sock);
#else
            close(sock);
#endif
            return false;
        }

        int result = ::connect(sock, reinterpret_cast<sockaddr*>(&address), sizeof(address));

        if (result == 0) {
            socket_handle_ = static_cast<std::uintptr_t>(sock);
            std::cerr << "frame_socket_connected:" << host << ":" << port << std::endl;
            std::cerr << "frame_socket_format:raw_bgr" << std::endl;
            return true;
        }

#ifdef _WIN32
        closesocket(sock);
#else
        close(sock);
#endif

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cerr << "frame_socket_connect_failed:" << host << ":" << port << std::endl;
    return false;
}

void VideoSource::close_socket() {
    if (socket_handle_ == 0) {
        return;
    }

#ifdef _WIN32
    closesocket(static_cast<SOCKET>(socket_handle_));
    WSACleanup();
#else
    close(static_cast<int>(socket_handle_));
#endif

    socket_handle_ = 0;
}

bool VideoSource::recv_all(unsigned char* data, std::size_t size) {
    std::size_t received = 0;

    while (received < size) {
#ifdef _WIN32
        int count = recv(
            static_cast<SOCKET>(socket_handle_),
            reinterpret_cast<char*>(data + received),
            static_cast<int>(size - received),
            0
        );
#else
        ssize_t count = recv(
            static_cast<int>(socket_handle_),
            data + received,
            size - received,
            0
        );
#endif

        if (count <= 0) {
            return false;
        }

        received += static_cast<std::size_t>(count);
    }

    return true;
}

std::optional<Frame> VideoSource::read_socket_frame() {
    unsigned char header[20];

    if (!recv_all(header, sizeof(header))) {
        opened_ = false;
        return std::nullopt;
    }

    if (!(header[0] == 'V' && header[1] == 'C' && header[2] == 'M' && header[3] == 'R')) {
        std::cerr << "invalid_raw_frame_magic" << std::endl;
        opened_ = false;
        return std::nullopt;
    }

    std::uint32_t width = read_u32_be(header + 4);
    std::uint32_t height = read_u32_be(header + 8);
    std::uint32_t channels = read_u32_be(header + 12);
    std::uint32_t payload_size = read_u32_be(header + 16);

    if (width == 0 || height == 0 || channels != 3) {
        std::cerr << "invalid_raw_frame_shape:" << width << "x" << height << "x" << channels << std::endl;
        opened_ = false;
        return std::nullopt;
    }

    std::uint64_t expected_size =
        static_cast<std::uint64_t>(width) *
        static_cast<std::uint64_t>(height) *
        static_cast<std::uint64_t>(channels);

    if (expected_size != payload_size || payload_size > 64 * 1024 * 1024) {
        std::cerr << "invalid_raw_frame_payload_size:"
                  << payload_size
                  << " expected:"
                  << expected_size
                  << std::endl;
        opened_ = false;
        return std::nullopt;
    }

    std::vector<unsigned char> buffer(payload_size);

    if (!recv_all(buffer.data(), buffer.size())) {
        opened_ = false;
        return std::nullopt;
    }

    cv::Mat image(
        static_cast<int>(height),
        static_cast<int>(width),
        CV_8UC3,
        buffer.data()
    );

    cv::Mat owned_image = image.clone();

    return mat_to_frame(owned_image, frame_id_++);
}