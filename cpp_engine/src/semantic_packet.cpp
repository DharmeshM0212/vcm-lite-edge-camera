#include "semantic_packet.hpp"

#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>

static std::string json_escape(const std::string& text) {
    std::ostringstream output;

    for (char c : text) {
        switch (c) {
            case '"':
                output << "\\\"";
                break;
            case '\\':
                output << "\\\\";
                break;
            case '\n':
                output << "\\n";
                break;
            case '\r':
                output << "\\r";
                break;
            case '\t':
                output << "\\t";
                break;
            default:
                output << c;
                break;
        }
    }

    return output.str();
}

static void write_u32_be(std::ofstream& file, std::uint32_t value) {
    unsigned char bytes[4];
    bytes[0] = static_cast<unsigned char>((value >> 24) & 0xFF);
    bytes[1] = static_cast<unsigned char>((value >> 16) & 0xFF);
    bytes[2] = static_cast<unsigned char>((value >> 8) & 0xFF);
    bytes[3] = static_cast<unsigned char>(value & 0xFF);
    file.write(reinterpret_cast<const char*>(bytes), 4);
}

std::string semantic_packet_metadata_json(const SemanticPacketInput& input) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(3);

    ss << "{";
    ss << "\"packet_type\":\"vcm_lite_semantic_packet\",";
    ss << "\"version\":1,";
    ss << "\"frame_id\":" << input.frame_id << ",";
    ss << "\"timestamp_ms\":" << input.timestamp_ms << ",";
    ss << "\"source_frame\":{";
    ss << "\"width\":" << input.frame_width << ",";
    ss << "\"height\":" << input.frame_height;
    ss << "},";
    ss << "\"context_payload\":{";
    ss << "\"codec\":\"jpeg\",";
    ss << "\"width\":" << input.context_width << ",";
    ss << "\"height\":" << input.context_height << ",";
    ss << "\"quality\":" << input.context_quality << ",";
    ss << "\"bytes\":" << input.context_jpeg.bytes.size();
    ss << "},";
    ss << "\"roi_payload\":{";
    ss << "\"codec\":\"jpeg\",";
    ss << "\"width\":" << input.roi_tile_width << ",";
    ss << "\"height\":" << input.roi_tile_height << ",";
    ss << "\"quality\":" << input.roi_quality << ",";
    ss << "\"bytes\":" << input.roi_tile_jpeg.bytes.size();
    ss << "},";
    ss << "\"controller\":{";
    ss << "\"state\":\"" << json_escape(input.controller_state) << "\",";
    ss << "\"detector_interval\":" << input.detector_interval;
    ss << "},";
    ss << "\"machine_quality\":{";
    ss << "\"reference_ai_confidence\":" << input.reference_ai_confidence << ",";
    ss << "\"compressed_ai_confidence\":" << input.compressed_ai_confidence << ",";
    ss << "\"detector_confidence_loss\":" << input.detector_confidence_loss << ",";
    ss << "\"ai_stability_loss\":" << input.ai_stability_loss;
    ss << "},";
    ss << "\"roi_boxes\":[";

    for (std::size_t i = 0; i < input.roi_result.boxes.size(); i++) {
        const auto& box = input.roi_result.boxes[i];

        ss << "{";
        ss << "\"id\":" << i << ",";
        ss << "\"x\":" << box.x << ",";
        ss << "\"y\":" << box.y << ",";
        ss << "\"width\":" << box.width << ",";
        ss << "\"height\":" << box.height << ",";
        ss << "\"confidence\":" << box.confidence;
        ss << "}";

        if (i + 1 < input.roi_result.boxes.size()) {
            ss << ",";
        }
    }

    ss << "],";
    ss << "\"detected_objects\":[";

    for (std::size_t i = 0; i < input.detector_result.objects.size(); i++) {
        const auto& object = input.detector_result.objects[i];

        ss << "{";
        ss << "\"id\":" << i << ",";
        ss << "\"label\":\"" << json_escape(object.label) << "\",";
        ss << "\"class_id\":" << object.class_id << ",";
        ss << "\"confidence\":" << object.confidence << ",";
        ss << "\"x\":" << object.x << ",";
        ss << "\"y\":" << object.y << ",";
        ss << "\"width\":" << object.width << ",";
        ss << "\"height\":" << object.height;
        ss << "}";

        if (i + 1 < input.detector_result.objects.size()) {
            ss << ",";
        }
    }

    ss << "]";
    ss << "}";

    return ss.str();
}

bool write_semantic_packet(const SemanticPacketInput& input, const std::string& output_dir) {
    std::string metadata = semantic_packet_metadata_json(input);

    std::string json_path = output_dir + "/latest_semantic_packet.json";
    std::string bin_path = output_dir + "/latest_semantic_packet.bin";

    {
        std::ofstream json_file(json_path, std::ios::out | std::ios::trunc);

        if (!json_file.is_open()) {
            return false;
        }

        json_file << metadata;
    }

    std::ofstream bin_file(bin_path, std::ios::binary | std::ios::out | std::ios::trunc);

    if (!bin_file.is_open()) {
        return false;
    }

    const char magic[8] = {'V', 'C', 'M', 'L', 'I', 'T', 'E', '1'};
    bin_file.write(magic, 8);

    write_u32_be(bin_file, static_cast<std::uint32_t>(metadata.size()));
    write_u32_be(bin_file, static_cast<std::uint32_t>(input.context_jpeg.bytes.size()));
    write_u32_be(bin_file, static_cast<std::uint32_t>(input.roi_tile_jpeg.bytes.size()));

    bin_file.write(metadata.data(), static_cast<std::streamsize>(metadata.size()));

    if (!input.context_jpeg.bytes.empty()) {
        bin_file.write(
            reinterpret_cast<const char*>(input.context_jpeg.bytes.data()),
            static_cast<std::streamsize>(input.context_jpeg.bytes.size())
        );
    }

    if (!input.roi_tile_jpeg.bytes.empty()) {
        bin_file.write(
            reinterpret_cast<const char*>(input.roi_tile_jpeg.bytes.data()),
            static_cast<std::streamsize>(input.roi_tile_jpeg.bytes.size())
        );
    }

    return true;
}