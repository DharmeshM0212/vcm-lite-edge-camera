#pragma once

#include "frame.hpp"
#include "roi.hpp"

#include <opencv2/dnn.hpp>
#include <string>
#include <vector>

struct DetectedObject {
    int x;
    int y;
    int width;
    int height;
    double confidence;
    int class_id;
    std::string label;
};

struct DetectorResult {
    std::vector<DetectedObject> objects;
    double mean_confidence;
    bool used_dnn;
    bool detector_ran;
    int raw_candidate_count;
    double max_raw_confidence;
};

class MotionAsDetector {
public:
    DetectorResult detect(const Frame& frame, const RoiResult& roi_result) const;
};

class OpenCVDnnDetector {
public:
    OpenCVDnnDetector(const std::string& model_path, const std::string& labels_path);

    bool is_available() const;
    DetectorResult detect(const Frame& frame, const RoiResult& fallback_roi_result);

private:
    cv::dnn::Net net_;
    std::vector<std::string> labels_;
    bool available_;
    int input_width_;
    int input_height_;
    float confidence_threshold_;
    float nms_threshold_;

    DetectorResult detect_with_dnn(const Frame& frame);
    DetectorResult detect_with_motion_fallback(const Frame& frame, const RoiResult& roi_result) const;
    std::vector<std::string> load_labels(const std::string& labels_path) const;
};

RoiResult detector_result_to_roi_result(const Frame& frame, const DetectorResult& detector_result, double min_confidence, int padding_pixels);
RoiResult fuse_detector_and_motion_rois(const Frame& frame, const DetectorResult& detector_result, const RoiResult& motion_roi_result, int max_rois);
double detector_confidence_loss(double reference_confidence, double compressed_confidence);