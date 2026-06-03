#include "detection_event_writer.hpp"

#include "opencv_bridge.hpp"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <opencv2/opencv.hpp>
#include <sstream>

static DetectedObject best_object(const DetectorResult& detector_result) {
    DetectedObject best;
    best.x = 0;
    best.y = 0;
    best.width = 1;
    best.height = 1;
    best.confidence = 0.0;
    best.class_id = -1;
    best.label = "none";

    for (const auto& object : detector_result.objects) {
        if (object.confidence > best.confidence) {
            best = object;
        }
    }

    return best;
}

static void draw_objects(cv::Mat& image, const DetectorResult& detector_result) {
    for (const auto& object : detector_result.objects) {
        cv::Rect rect(
            std::max(0, object.x),
            std::max(0, object.y),
            std::max(1, object.width),
            std::max(1, object.height)
        );

        rect &= cv::Rect(0, 0, image.cols, image.rows);

        if (rect.width <= 0 || rect.height <= 0) {
            continue;
        }

        cv::rectangle(image, rect, cv::Scalar(0, 255, 0), 2);

        std::ostringstream label;
        label << object.label << " " << std::fixed << std::setprecision(2) << object.confidence;

        int baseline = 0;
        cv::Size text_size = cv::getTextSize(label.str(), cv::FONT_HERSHEY_SIMPLEX, 0.55, 1, &baseline);

        int text_x = rect.x;
        int text_y = std::max(0, rect.y - text_size.height - 6);

        cv::Rect bg_rect(
            text_x,
            text_y,
            std::min(text_size.width + 8, image.cols - text_x),
            text_size.height + baseline + 8
        );

        if (bg_rect.width > 0 && bg_rect.height > 0) {
            cv::rectangle(image, bg_rect, cv::Scalar(0, 120, 0), cv::FILLED);
            cv::putText(
                image,
                label.str(),
                cv::Point(text_x + 4, text_y + text_size.height + 2),
                cv::FONT_HERSHEY_SIMPLEX,
                0.55,
                cv::Scalar(255, 255, 255),
                1,
                cv::LINE_AA
            );
        }
    }
}

static cv::Mat crop_object(const cv::Mat& image, const DetectedObject& object, int padding) {
    int x = std::max(0, object.x - padding);
    int y = std::max(0, object.y - padding);
    int right = std::min(image.cols, object.x + object.width + padding);
    int bottom = std::min(image.rows, object.y + object.height + padding);

    int width = std::max(1, right - x);
    int height = std::max(1, bottom - y);

    cv::Rect rect(x, y, width, height);
    return image(rect).clone();
}

static bool write_event_json(
    const std::string& path,
    const DetectorResult& detector_result,
    const DetectedObject& object,
    std::uint64_t frame_id,
    double reference_ai_confidence,
    double compressed_ai_confidence,
    double detector_confidence_loss,
    double ai_stability_loss
) {
    std::ofstream file(path, std::ios::out | std::ios::trunc);

    if (!file.is_open()) {
        return false;
    }

    file << std::fixed << std::setprecision(3);
    file << "{";
    file << "\"frame_id\":" << frame_id << ",";
    file << "\"object_count\":" << detector_result.objects.size() << ",";
    file << "\"primary_object\":{";
    file << "\"label\":\"" << object.label << "\",";
    file << "\"class_id\":" << object.class_id << ",";
    file << "\"confidence\":" << object.confidence << ",";
    file << "\"x\":" << object.x << ",";
    file << "\"y\":" << object.y << ",";
    file << "\"width\":" << object.width << ",";
    file << "\"height\":" << object.height;
    file << "},";
    file << "\"reference_ai_confidence\":" << reference_ai_confidence << ",";
    file << "\"compressed_ai_confidence\":" << compressed_ai_confidence << ",";
    file << "\"detector_confidence_loss\":" << detector_confidence_loss << ",";
    file << "\"ai_stability_loss\":" << ai_stability_loss << ",";
    file << "\"objects\":[";

    for (std::size_t i = 0; i < detector_result.objects.size(); i++) {
        const auto& item = detector_result.objects[i];

        file << "{";
        file << "\"label\":\"" << item.label << "\",";
        file << "\"class_id\":" << item.class_id << ",";
        file << "\"confidence\":" << item.confidence << ",";
        file << "\"x\":" << item.x << ",";
        file << "\"y\":" << item.y << ",";
        file << "\"width\":" << item.width << ",";
        file << "\"height\":" << item.height;
        file << "}";

        if (i + 1 < detector_result.objects.size()) {
            file << ",";
        }
    }

    file << "]";
    file << "}";

    return true;
}

bool write_detection_event_snapshot(
    const Frame& reference_frame,
    const Frame& reconstructed_frame,
    const DetectorResult& detector_result,
    std::uint64_t frame_id,
    double reference_ai_confidence,
    double compressed_ai_confidence,
    double detector_confidence_loss,
    double ai_stability_loss,
    const std::string& output_dir
) {
    if (detector_result.objects.empty()) {
        return false;
    }

    cv::Mat reference = frame_to_mat(reference_frame);
    cv::Mat reconstructed = frame_to_mat(reconstructed_frame);

    if (reference.empty() || reconstructed.empty()) {
        return false;
    }

    DetectedObject object = best_object(detector_result);

    cv::Mat annotated_reference = reference.clone();
    cv::Mat annotated_reconstructed = reconstructed.clone();
    cv::Mat crop = crop_object(reference, object, 16);

    draw_objects(annotated_reference, detector_result);
    draw_objects(annotated_reconstructed, detector_result);

    bool ok = true;
    ok = cv::imwrite(output_dir + "/latest_detection_frame.jpg", annotated_reference) && ok;
    ok = cv::imwrite(output_dir + "/latest_detection_crop.jpg", crop) && ok;
    ok = cv::imwrite(output_dir + "/latest_detection_reconstructed.jpg", annotated_reconstructed) && ok;
    ok = write_event_json(
        output_dir + "/latest_detection_event.json",
        detector_result,
        object,
        frame_id,
        reference_ai_confidence,
        compressed_ai_confidence,
        detector_confidence_loss,
        ai_stability_loss
    ) && ok;

    return ok;
}