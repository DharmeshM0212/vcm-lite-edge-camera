#include "ai_detector.hpp"

#include "opencv_bridge.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <opencv2/imgproc.hpp>

struct LetterboxResult {
    cv::Mat image;
    double scale;
    int pad_x;
    int pad_y;
};

static LetterboxResult letterbox(const cv::Mat& input, int target_width, int target_height) {
    int input_width = input.cols;
    int input_height = input.rows;

    double scale = std::min(
        static_cast<double>(target_width) / static_cast<double>(input_width),
        static_cast<double>(target_height) / static_cast<double>(input_height)
    );

    int resized_width = std::max(1, static_cast<int>(std::round(static_cast<double>(input_width) * scale)));
    int resized_height = std::max(1, static_cast<int>(std::round(static_cast<double>(input_height) * scale)));

    cv::Mat resized;
    cv::resize(input, resized, cv::Size(resized_width, resized_height), 0.0, 0.0, cv::INTER_LINEAR);

    int pad_x = (target_width - resized_width) / 2;
    int pad_y = (target_height - resized_height) / 2;

    cv::Mat output(target_height, target_width, input.type(), cv::Scalar(114, 114, 114));
    cv::Rect roi(pad_x, pad_y, resized_width, resized_height);
    resized.copyTo(output(roi));

    LetterboxResult result;
    result.image = output;
    result.scale = scale;
    result.pad_x = pad_x;
    result.pad_y = pad_y;
    return result;
}

DetectorResult MotionAsDetector::detect(const Frame& frame, const RoiResult& roi_result) const {
    DetectorResult result;
    result.mean_confidence = 0.0;
    result.used_dnn = false;
    result.detector_ran = true;
    result.raw_candidate_count = static_cast<int>(roi_result.boxes.size());
    result.max_raw_confidence = 0.0;

    double confidence_sum = 0.0;

    for (const auto& box : roi_result.boxes) {
        DetectedObject object;
        object.x = box.x;
        object.y = box.y;
        object.width = box.width;
        object.height = box.height;
        object.confidence = std::clamp(box.confidence, 0.0, 1.0);
        object.class_id = 0;
        object.label = "motion_roi";

        result.objects.push_back(object);
        confidence_sum += object.confidence;
        result.max_raw_confidence = std::max(result.max_raw_confidence, object.confidence);
    }

    if (!result.objects.empty()) {
        result.mean_confidence = confidence_sum / static_cast<double>(result.objects.size());
    }

    return result;
}

OpenCVDnnDetector::OpenCVDnnDetector(const std::string& model_path, const std::string& labels_path)
    : available_(false),
      input_width_(320),
      input_height_(320),
      confidence_threshold_(0.15f),
      nms_threshold_(0.45f) {
    std::ifstream model_file(model_path, std::ios::binary);

    if (!model_file.good()) {
        std::cerr << "dnn_model_not_found:" << model_path << std::endl;
        return;
    }

    try {
        net_ = cv::dnn::readNetFromONNX(model_path);
        labels_ = load_labels(labels_path);
        available_ = !net_.empty();

        if (available_) {
            std::cerr << "dnn_model_loaded:" << model_path << std::endl;
        } else {
            std::cerr << "dnn_model_empty:" << model_path << std::endl;
        }
    } catch (const std::exception& error) {
        available_ = false;
        std::cerr << "dnn_model_load_failed:" << error.what() << std::endl;
    }
}

bool OpenCVDnnDetector::is_available() const {
    return available_;
}

DetectorResult OpenCVDnnDetector::detect(const Frame& frame, const RoiResult& fallback_roi_result) {
    if (!available_) {
        return detect_with_motion_fallback(frame, fallback_roi_result);
    }

    try {
        return detect_with_dnn(frame);
    } catch (const std::exception& error) {
        available_ = false;
        std::cerr << "dnn_inference_failed:" << error.what() << std::endl;
        return detect_with_motion_fallback(frame, fallback_roi_result);
    }
}

DetectorResult OpenCVDnnDetector::detect_with_motion_fallback(const Frame& frame, const RoiResult& roi_result) const {
    MotionAsDetector fallback;
    return fallback.detect(frame, roi_result);
}

std::vector<std::string> OpenCVDnnDetector::load_labels(const std::string& labels_path) const {
    std::vector<std::string> labels;
    std::ifstream file(labels_path);

    if (!file.good()) {
        return labels;
    }

    std::string line;

    while (std::getline(file, line)) {
        if (!line.empty()) {
            labels.push_back(line);
        }
    }

    return labels;
}

DetectorResult OpenCVDnnDetector::detect_with_dnn(const Frame& frame) {
    cv::Mat input = frame_to_mat(frame);

    DetectorResult result;
    result.mean_confidence = 0.0;
    result.used_dnn = true;
    result.detector_ran = true;
    result.raw_candidate_count = 0;
    result.max_raw_confidence = 0.0;

    if (input.empty()) {
        return result;
    }

    int original_width = input.cols;
    int original_height = input.rows;

    LetterboxResult boxed = letterbox(input, input_width_, input_height_);

    cv::Mat blob = cv::dnn::blobFromImage(
        boxed.image,
        1.0 / 255.0,
        cv::Size(input_width_, input_height_),
        cv::Scalar(),
        true,
        false
    );

    net_.setInput(blob);

    std::vector<cv::Mat> outputs;
    net_.forward(outputs, net_.getUnconnectedOutLayersNames());

    if (outputs.empty()) {
        return result;
    }

    cv::Mat output = outputs[0];
    cv::Mat detections;

    if (output.dims == 3) {
        int dim1 = output.size[1];
        int dim2 = output.size[2];

        if (dim1 <= 256 && dim2 > dim1) {
            cv::Mat raw(dim1, dim2, CV_32F, output.ptr<float>());
            detections = raw.t();
        } else {
            detections = cv::Mat(dim1, dim2, CV_32F, output.ptr<float>());
        }
    } else if (output.dims == 2) {
        detections = output;
    } else {
        return result;
    }

    int rows = detections.rows;
    int dimensions = detections.cols;

    std::vector<cv::Rect> boxes;
    std::vector<float> confidences;
    std::vector<int> class_ids;

    for (int i = 0; i < rows; i++) {
        const float* data = detections.ptr<float>(i);

        if (dimensions < 6) {
            continue;
        }

        float x = data[0];
        float y = data[1];
        float w = data[2];
        float h = data[3];

        int class_id = 0;
        float confidence = 0.0f;

        if (dimensions > 6) {
            for (int c = 4; c < dimensions; c++) {
                if (data[c] > confidence) {
                    confidence = data[c];
                    class_id = c - 4;
                }
            }
        } else {
            confidence = data[4];
            class_id = static_cast<int>(data[5]);
        }

        result.max_raw_confidence = std::max(result.max_raw_confidence, static_cast<double>(confidence));

        if (confidence < confidence_threshold_) {
            continue;
        }

        result.raw_candidate_count++;

        double left_boxed = static_cast<double>(x) - 0.5 * static_cast<double>(w);
        double top_boxed = static_cast<double>(y) - 0.5 * static_cast<double>(h);
        double width_boxed = static_cast<double>(w);
        double height_boxed = static_cast<double>(h);

        bool normalized = x <= 2.0f && y <= 2.0f && w <= 2.0f && h <= 2.0f;

        if (normalized) {
            left_boxed *= static_cast<double>(input_width_);
            top_boxed *= static_cast<double>(input_height_);
            width_boxed *= static_cast<double>(input_width_);
            height_boxed *= static_cast<double>(input_height_);
        }

        double left_original = (left_boxed - static_cast<double>(boxed.pad_x)) / boxed.scale;
        double top_original = (top_boxed - static_cast<double>(boxed.pad_y)) / boxed.scale;
        double width_original = width_boxed / boxed.scale;
        double height_original = height_boxed / boxed.scale;

        int left = static_cast<int>(std::round(left_original));
        int top = static_cast<int>(std::round(top_original));
        int width = static_cast<int>(std::round(width_original));
        int height = static_cast<int>(std::round(height_original));

        left = std::clamp(left, 0, std::max(0, original_width - 1));
        top = std::clamp(top, 0, std::max(0, original_height - 1));
        width = std::clamp(width, 1, std::max(1, original_width - left));
        height = std::clamp(height, 1, std::max(1, original_height - top));

        boxes.emplace_back(left, top, width, height);
        confidences.push_back(confidence);
        class_ids.push_back(class_id);
    }

    std::vector<int> indices;
    cv::dnn::NMSBoxes(boxes, confidences, confidence_threshold_, nms_threshold_, indices);

    double confidence_sum = 0.0;

    for (int index : indices) {
        const cv::Rect& rect = boxes[static_cast<std::size_t>(index)];

        DetectedObject object;
        object.x = rect.x;
        object.y = rect.y;
        object.width = rect.width;
        object.height = rect.height;
        object.confidence = confidences[static_cast<std::size_t>(index)];
        object.class_id = class_ids[static_cast<std::size_t>(index)];

        if (object.class_id >= 0 && object.class_id < static_cast<int>(labels_.size())) {
            object.label = labels_[static_cast<std::size_t>(object.class_id)];
        } else {
            object.label = "object";
        }

        result.objects.push_back(object);
        confidence_sum += object.confidence;

        if (result.objects.size() >= 20) {
            break;
        }
    }

    if (!result.objects.empty()) {
        result.mean_confidence = confidence_sum / static_cast<double>(result.objects.size());
    }

    return result;
}

double detector_confidence_loss(double reference_confidence, double compressed_confidence) {
    return std::clamp(std::abs(reference_confidence - compressed_confidence), 0.0, 1.0);
}