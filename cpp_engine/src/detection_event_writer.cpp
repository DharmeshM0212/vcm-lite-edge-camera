#include "detection_event_writer.hpp"

#include "opencv_bridge.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <opencv2/opencv.hpp>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

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

static cv::Rect clamp_rect(const cv::Rect& rect, int width, int height) {
    int x = std::max(0, rect.x);
    int y = std::max(0, rect.y);
    int right = std::min(width, rect.x + rect.width);
    int bottom = std::min(height, rect.y + rect.height);

    int w = std::max(0, right - x);
    int h = std::max(0, bottom - y);

    return cv::Rect(x, y, w, h);
}

static cv::Rect object_rect(const DetectedObject& object) {
    return cv::Rect(object.x, object.y, object.width, object.height);
}

static int best_object_index(const DetectorResult& detector_result) {
    if (detector_result.objects.empty()) {
        return -1;
    }

    int best_index = 0;
    double best_confidence = detector_result.objects[0].confidence;

    for (std::size_t i = 1; i < detector_result.objects.size(); i++) {
        if (detector_result.objects[i].confidence > best_confidence) {
            best_confidence = detector_result.objects[i].confidence;
            best_index = static_cast<int>(i);
        }
    }

    return best_index;
}

static void draw_object(cv::Mat& image, const DetectedObject& object) {
    cv::Rect rect = clamp_rect(object_rect(object), image.cols, image.rows);

    if (rect.width <= 0 || rect.height <= 0) {
        return;
    }

    cv::Scalar box_color(0, 255, 0);
    cv::Scalar fill_color(0, 128, 0);
    cv::Scalar text_color(255, 255, 255);

    cv::rectangle(image, rect, box_color, 2);

    std::ostringstream label;
    label << object.label << " " << std::fixed << std::setprecision(2) << object.confidence;

    int baseline = 0;
    cv::Size text_size = cv::getTextSize(label.str(), cv::FONT_HERSHEY_SIMPLEX, 0.55, 1, &baseline);

    int label_x = rect.x;
    int label_y = std::max(0, rect.y - text_size.height - 8);

    cv::Rect label_bg(
        label_x,
        label_y,
        std::min(text_size.width + 8, image.cols - label_x),
        text_size.height + baseline + 8
    );

    if (label_bg.width > 0 && label_bg.height > 0) {
        cv::rectangle(image, label_bg, fill_color, cv::FILLED);
        cv::putText(
            image,
            label.str(),
            cv::Point(label_x + 4, label_y + text_size.height + 2),
            cv::FONT_HERSHEY_SIMPLEX,
            0.55,
            text_color,
            1,
            cv::LINE_AA
        );
    }
}

static void draw_objects(cv::Mat& image, const DetectorResult& detector_result) {
    for (const auto& object : detector_result.objects) {
        draw_object(image, object);
    }
}

static std::string object_json(const DetectedObject& object, int id) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(3);

    ss << "{";
    ss << "\"id\":" << id << ",";
    ss << "\"label\":\"" << json_escape(object.label) << "\",";
    ss << "\"class_id\":" << object.class_id << ",";
    ss << "\"confidence\":" << object.confidence << ",";
    ss << "\"x\":" << object.x << ",";
    ss << "\"y\":" << object.y << ",";
    ss << "\"width\":" << object.width << ",";
    ss << "\"height\":" << object.height;
    ss << "}";

    return ss.str();
}

static std::string event_json(
    const DetectorResult& detector_result,
    std::uint64_t frame_id,
    double reference_ai_confidence,
    double compressed_ai_confidence,
    double detector_confidence_loss,
    double task_preservation_loss,
    double semantic_psnr_db,
    double semantic_ssim
) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(3);

    int best_index = best_object_index(detector_result);

    ss << "{";
    ss << "\"event_type\":\"object_detection\",";
    ss << "\"frame_id\":" << frame_id << ",";
    ss << "\"object_count\":" << detector_result.objects.size() << ",";
    ss << "\"reference_ai_confidence\":" << reference_ai_confidence << ",";
    ss << "\"compressed_ai_confidence\":" << compressed_ai_confidence << ",";
    ss << "\"detector_confidence_loss\":" << detector_confidence_loss << ",";
    ss << "\"task_preservation_loss\":" << task_preservation_loss << ",";
    ss << "\"ai_stability_loss\":" << task_preservation_loss << ",";
    ss << "\"semantic_psnr_db\":" << semantic_psnr_db << ",";
    ss << "\"semantic_ssim\":" << semantic_ssim << ",";
    ss << "\"detector_used_dnn\":" << (detector_result.used_dnn ? "true" : "false") << ",";
    ss << "\"raw_candidate_count\":" << detector_result.raw_candidate_count << ",";
    ss << "\"max_raw_confidence\":" << detector_result.max_raw_confidence << ",";

    ss << "\"primary_object\":";
    if (best_index >= 0) {
        ss << object_json(detector_result.objects[static_cast<std::size_t>(best_index)], best_index);
    } else {
        ss << "{}";
    }

    ss << ",";
    ss << "\"objects\":[";

    for (std::size_t i = 0; i < detector_result.objects.size(); i++) {
        ss << object_json(detector_result.objects[i], static_cast<int>(i));

        if (i + 1 < detector_result.objects.size()) {
            ss << ",";
        }
    }

    ss << "]";
    ss << "}";

    return ss.str();
}

static void append_jsonl(const fs::path& path, const std::string& line) {
    std::ofstream file(path, std::ios::out | std::ios::app);

    if (!file.is_open()) {
        return;
    }

    file << line << "\n";
}

static void write_text_file(const fs::path& path, const std::string& text) {
    std::ofstream file(path, std::ios::out | std::ios::trunc);

    if (!file.is_open()) {
        return;
    }

    file << text;
}

bool write_detection_event_snapshot(
    const Frame& reference_frame,
    const Frame& reconstructed_frame,
    const DetectorResult& detector_result,
    std::uint64_t frame_id,
    double reference_ai_confidence,
    double compressed_ai_confidence,
    double detector_confidence_loss,
    double task_preservation_loss,
    double semantic_psnr_db,
    double semantic_ssim,
    const std::string& output_dir
) {
    if (detector_result.objects.empty()) {
        return false;
    }

    fs::path output_path(output_dir);
    fs::create_directories(output_path);

    cv::Mat reference_image = frame_to_mat(reference_frame);
    cv::Mat reconstructed_image = frame_to_mat(reconstructed_frame);

    if (reference_image.empty() || reconstructed_image.empty()) {
        return false;
    }

    cv::Mat annotated_reference = reference_image.clone();
    cv::Mat annotated_reconstructed = reconstructed_image.clone();

    draw_objects(annotated_reference, detector_result);
    draw_objects(annotated_reconstructed, detector_result);

    int best_index = best_object_index(detector_result);

    if (best_index < 0) {
        return false;
    }

    const DetectedObject& primary = detector_result.objects[static_cast<std::size_t>(best_index)];
    cv::Rect crop_rect = clamp_rect(object_rect(primary), reference_image.cols, reference_image.rows);

    if (crop_rect.width <= 0 || crop_rect.height <= 0) {
        return false;
    }

    cv::Mat crop = reference_image(crop_rect).clone();

    cv::imwrite((output_path / "latest_detection_frame.jpg").string(), annotated_reference);
    cv::imwrite((output_path / "latest_detection_crop.jpg").string(), crop);
    cv::imwrite((output_path / "latest_detection_reconstructed.jpg").string(), annotated_reconstructed);

    std::string json = event_json(
        detector_result,
        frame_id,
        reference_ai_confidence,
        compressed_ai_confidence,
        detector_confidence_loss,
        task_preservation_loss,
        semantic_psnr_db,
        semantic_ssim
    );

    write_text_file(output_path / "latest_detection_event.json", json);
    append_jsonl(output_path / "detection_history.jsonl", json);

    return true;
}