#include "ai_detector.hpp"
#include "config.hpp"
#include "frame.hpp"
#include "frame_queue.hpp"
#include "image_statistics.hpp"
#include "isp_tuning.hpp"
#include "jpeg_encoder.hpp"
#include "log_writer.hpp"
#include "metadata.hpp"
#include "metrics.hpp"
#include "motion_roi.hpp"
#include "rate_controller.hpp"
#include "roi.hpp"
#include "roi_packer.hpp"
#include "semantic_encoder.hpp"
#include "semantic_reconstruct.hpp"
#include "stability.hpp"
#include "video_source.hpp"
#include "output_writer.hpp"
#include "detection_event_writer.hpp"
#include "system_monitor.hpp"
#include "semantic_packet.hpp"
#include "runtime_schedule.hpp"
#include "target_filter.hpp"
#include "opencv_bridge.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <opencv2/opencv.hpp>
#include <sstream>
#include <string>
#include <thread>

namespace fs = std::filesystem;

static double elapsed_ms(
    const std::chrono::steady_clock::time_point& start,
    const std::chrono::steady_clock::time_point& end
) {
    return std::chrono::duration<double, std::milli>(end - start).count();
}

static std::string json_escape_local(const std::string& text) {
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

static void write_text_file_local(const fs::path& path, const std::string& text) {
    std::ofstream file(path, std::ios::out | std::ios::trunc);

    if (!file.is_open()) {
        return;
    }

    file << text;
}

static void append_jsonl_local(const fs::path& path, const std::string& text) {
    std::ofstream file(path, std::ios::out | std::ios::app);

    if (!file.is_open()) {
        return;
    }

    file << text << "\n";
}

static IspSettings apply_realtime_isp_guard(
    IspSettings settings,
    const RateControllerOutput& controller_output,
    double previous_ai_stability_loss,
    double confidence_loss_threshold
) {
    std::string state = controller_output.controller_state;
    std::string mode = controller_output.controller_mode;

    bool realtime_pressure =
        state.find("overload") != std::string::npos ||
        mode.find("realtime") != std::string::npos;

    bool ai_pressure = previous_ai_stability_loss > 0.60 * confidence_loss_threshold;

    if (!realtime_pressure && !ai_pressure) {
        return settings;
    }

    if (realtime_pressure) {
        settings.clahe_enabled = false;
        settings.denoise_strength = std::min(settings.denoise_strength, 0.0);
        settings.sharpen_amount = std::min(settings.sharpen_amount, 0.20);

        if (settings.profile.find("_rt") == std::string::npos) {
            settings.profile += "_rt";
        }
    }

    if (ai_pressure) {
        settings.sharpen_amount = std::max(settings.sharpen_amount, 0.25);
        settings.contrast_alpha = std::max(settings.contrast_alpha, 1.05);
        settings.clahe_enabled = false;

        if (settings.profile.find("_ai_protect") == std::string::npos) {
            settings.profile += "_ai_protect";
        }
    }

    return settings;
}

static std::string validation_json(
    std::uint64_t frame_id,
    const std::string& validation_type,
    double reference_ai_confidence,
    double compressed_ai_confidence,
    double detector_confidence_loss,
    double ai_stability_loss,
    bool compressed_validation_ran,
    bool detector_ran
) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(3);

    ss << "{";
    ss << "\"frame_id\":" << frame_id << ",";
    ss << "\"validation_type\":\"" << json_escape_local(validation_type) << "\",";
    ss << "\"reference_ai_confidence\":" << reference_ai_confidence << ",";
    ss << "\"compressed_ai_confidence\":" << compressed_ai_confidence << ",";
    ss << "\"detector_confidence_loss\":" << detector_confidence_loss << ",";
    ss << "\"ai_stability_loss\":" << ai_stability_loss << ",";
    ss << "\"compressed_validation_ran\":" << (compressed_validation_ran ? "true" : "false") << ",";
    ss << "\"detector_ran\":" << (detector_ran ? "true" : "false");
    ss << "}";

    return ss.str();
}

static bool write_validation_snapshot(
    const Frame& tuned_frame,
    const Frame& reconstructed_frame,
    std::uint64_t frame_id,
    const std::string& validation_type,
    double reference_ai_confidence,
    double compressed_ai_confidence,
    double detector_confidence_loss,
    double ai_stability_loss,
    bool compressed_validation_ran,
    bool detector_ran,
    const std::string& output_dir
) {
    fs::path output_path(output_dir);
    fs::create_directories(output_path);

    cv::Mat tuned_image = frame_to_mat(tuned_frame);
    cv::Mat reconstructed_image = frame_to_mat(reconstructed_frame);

    if (tuned_image.empty() || reconstructed_image.empty()) {
        return false;
    }

    cv::imwrite((output_path / "latest_validation_frame.jpg").string(), tuned_image);
    cv::imwrite((output_path / "latest_validation_reconstructed.jpg").string(), reconstructed_image);

    std::string json = validation_json(
        frame_id,
        validation_type,
        reference_ai_confidence,
        compressed_ai_confidence,
        detector_confidence_loss,
        ai_stability_loss,
        compressed_validation_ran,
        detector_ran
    );

    write_text_file_local(output_path / "latest_validation.json", json);
    append_jsonl_local(output_path / "validation_history.jsonl", json);

    return true;
}

int main(int argc, char** argv) {
    std::string video_path = "../assets/videos/input.mp4";
    std::string metrics_log_path = "../../logs/metrics.jsonl";
    std::string metadata_log_path = "../../logs/metadata.jsonl";
    std::string model_path = "../../models/object_detector.onnx";
    std::string labels_path = "../../models/labels.txt";
    std::string output_dir = "../../outputs";

    if (argc >= 2) {
        video_path = argv[1];
    }

    if (argc >= 3) {
        metrics_log_path = argv[2];
    }

    if (argc >= 4) {
        metadata_log_path = argv[3];
    }

    if (argc >= 5) {
        model_path = argv[4];
    }

    if (argc >= 6) {
        labels_path = argv[5];
    }

    if (argc >= 7) {
        output_dir = argv[6];
    }

    VideoSource source(video_path);

    if (!source.is_opened()) {
        std::cerr << "failed_to_open_video:" << video_path << std::endl;
        return 1;
    }

    JsonlLogWriter metrics_log(metrics_log_path);
    JsonlLogWriter metadata_log(metadata_log_path);

    if (!metrics_log.is_open()) {
        std::cerr << "failed_to_open_metrics_log:" << metrics_log_path << std::endl;
        return 1;
    }

    if (!metadata_log.is_open()) {
        std::cerr << "failed_to_open_metadata_log:" << metadata_log_path << std::endl;
        return 1;
    }

    EngineConfig config = default_config();
    FrameQueue queue(3);
    MotionRoiDetector motion_detector;
    OpenCVDnnDetector ai_detector(model_path, labels_path);
    RateController rate_controller(config);

    TargetFilterPolicy roi_filter_policy = target_filter_roi_policy();
    TargetFilterPolicy event_filter_policy = target_filter_event_policy();

    std::atomic<bool> running(true);

    double source_fps = source.fps();

    if (source_fps <= 1.0 || source_fps > 240.0) {
        source_fps = 30.0;
    }

    int source_delay_ms = static_cast<int>(1000.0 / source_fps);

    std::thread producer([&]() {
        while (running.load()) {
            auto maybe_frame = source.read();

            if (!maybe_frame.has_value()) {
                running.store(false);
                break;
            }

            queue.push(std::move(*maybe_frame));
            std::this_thread::sleep_for(std::chrono::milliseconds(source_delay_ms));
        }
    });

    std::thread consumer([&]() {
        auto fps_window_start = std::chrono::steady_clock::now();
        std::uint64_t fps_window_count = 0;
        double measured_fps = 0.0;
        double previous_bitrate_kbps = 0.0;
        double previous_ai_stability_loss = 0.0;

        RuntimeSchedule runtime_schedule = default_runtime_schedule();

        double cached_reference_ai_confidence = 0.0;
        double cached_compressed_ai_confidence = 0.0;
        double cached_detector_confidence_loss = 0.0;
        double cached_ai_stability_loss = 0.0;

        ProcessMonitor process_monitor;

        DetectorResult last_reference_detector_result;
        last_reference_detector_result.mean_confidence = 0.0;
        last_reference_detector_result.used_dnn = false;
        last_reference_detector_result.detector_ran = false;
        last_reference_detector_result.raw_candidate_count = 0;
        last_reference_detector_result.max_raw_confidence = 0.0;

        DetectorResult last_event_detector_result;
        last_event_detector_result.mean_confidence = 0.0;
        last_event_detector_result.used_dnn = false;
        last_event_detector_result.detector_ran = false;
        last_event_detector_result.raw_candidate_count = 0;
        last_event_detector_result.max_raw_confidence = 0.0;

        DetectorResult last_compressed_detector_result;
        last_compressed_detector_result.mean_confidence = 0.0;
        last_compressed_detector_result.used_dnn = false;
        last_compressed_detector_result.detector_ran = false;
        last_compressed_detector_result.raw_candidate_count = 0;
        last_compressed_detector_result.max_raw_confidence = 0.0;

        std::uint64_t last_detector_frame_id = 0;
        bool has_detector_frame = false;

        RateControllerOutput controller_output;
        controller_output.roi_quality = 85;
        controller_output.context_quality = 35;
        controller_output.detector_interval = config.detector_interval;
        controller_output.context_width = 320;
        controller_output.roi_cell_size = 160;
        controller_output.max_rois = 5;
        controller_output.reencode_allowed = true;
        controller_output.controller_state = "initial";
        controller_output.controller_mode = "initial";
        controller_output.controller_reason = "engine startup";
        controller_output.controller_action = "initialize semantic controller defaults";

        while (running.load() || queue.size() > 0) {
            auto maybe_frame = queue.pop();

            if (maybe_frame.has_value()) {
                fps_window_count++;
                auto fps_now = std::chrono::steady_clock::now();
                auto fps_elapsed = std::chrono::duration<double>(fps_now - fps_window_start).count();

                if (fps_elapsed >= 1.0) {
                    measured_fps = static_cast<double>(fps_window_count) / fps_elapsed;
                    fps_window_count = 0;
                    fps_window_start = fps_now;
                }

                auto frame_start = std::chrono::steady_clock::now();
                double queue_wait_ms = elapsed_ms(maybe_frame->created_at, frame_start);

                double isp_ms = 0.0;
                double motion_roi_ms = 0.0;
                double detector_ms = 0.0;
                double semantic_encode_ms = 0.0;
                double semantic_reconstruct_ms = 0.0;
                double compressed_validation_ms = 0.0;
                double event_write_ms = 0.0;

                auto isp_start = std::chrono::steady_clock::now();
                ImageStats input_stats = compute_image_stats(*maybe_frame);
                IspSettings isp_settings = choose_isp_settings(input_stats);
                isp_settings = apply_realtime_isp_guard(
                    isp_settings,
                    controller_output,
                    previous_ai_stability_loss,
                    config.confidence_loss_threshold
                );
                Frame tuned_frame = apply_isp_tuning(*maybe_frame, isp_settings);
                ImageStats output_stats = compute_image_stats(tuned_frame);
                auto isp_end = std::chrono::steady_clock::now();
                isp_ms = elapsed_ms(isp_start, isp_end);

                auto motion_start = std::chrono::steady_clock::now();
                RoiResult motion_roi_result = motion_detector.detect(tuned_frame);
                auto motion_end = std::chrono::steady_clock::now();
                motion_roi_ms = elapsed_ms(motion_start, motion_end);

                int effective_detector_interval = std::max(
                    controller_output.detector_interval,
                    runtime_schedule.minimum_detector_interval
                );

                if (controller_output.controller_mode == "realtime" ||
                    controller_output.controller_mode == "realtime_protect") {
                    effective_detector_interval = std::max(
                        effective_detector_interval,
                        runtime_schedule.overload_detector_interval
                    );
                }

                bool warmup_detector =
                    maybe_frame->id < static_cast<std::uint64_t>(runtime_schedule.warmup_detector_frames);

                bool scheduled_detector =
                    maybe_frame->id % static_cast<std::uint64_t>(std::max(1, effective_detector_interval)) == 0;

                bool force_detector_on_ai_loss =
                    previous_ai_stability_loss > 0.75 * config.confidence_loss_threshold;

                bool detector_ran_this_frame =
                    warmup_detector || scheduled_detector || force_detector_on_ai_loss;

                if (detector_ran_this_frame) {
                    auto detector_start = std::chrono::steady_clock::now();

                    DetectorResult raw_reference_detector_result =
                        ai_detector.detect(tuned_frame, motion_roi_result);

                    last_reference_detector_result = filter_task_relevant_objects(
                        raw_reference_detector_result,
                        roi_filter_policy
                    );

                    last_event_detector_result = filter_task_relevant_objects(
                        raw_reference_detector_result,
                        event_filter_policy
                    );

                    cached_reference_ai_confidence = last_reference_detector_result.mean_confidence;
                    last_detector_frame_id = maybe_frame->id;
                    has_detector_frame = true;

                    auto detector_end = std::chrono::steady_clock::now();
                    detector_ms = elapsed_ms(detector_start, detector_end);
                }

                std::uint64_t detector_age =
                    has_detector_frame ? maybe_frame->id - last_detector_frame_id : 999999;

                bool object_roi_fresh =
                    has_detector_frame &&
                    detector_age <= static_cast<std::uint64_t>(runtime_schedule.object_roi_reuse_frames) &&
                    !last_reference_detector_result.objects.empty();

                DetectorResult roi_source_detector_result = last_reference_detector_result;

                if (!object_roi_fresh) {
                    roi_source_detector_result.objects.clear();
                    roi_source_detector_result.mean_confidence = 0.0;
                    roi_source_detector_result.raw_candidate_count = 0;
                    roi_source_detector_result.max_raw_confidence = 0.0;
                }

                RoiResult roi_result = fuse_detector_and_motion_rois(
                    tuned_frame,
                    roi_source_detector_result,
                    motion_roi_result,
                    5
                );

                double current_roi_area_ratio = roi_area_ratio(tuned_frame, roi_result);

                RateControllerInput controller_input;
                controller_input.latency_ms = queue_wait_ms;
                controller_input.roi_area_ratio = current_roi_area_ratio;
                controller_input.estimated_fps = measured_fps > 0.0 ? measured_fps : source_fps;
                controller_input.previous_bitrate_kbps = previous_bitrate_kbps;
                controller_input.previous_ai_stability_loss = previous_ai_stability_loss;
                controller_input.queue_depth = static_cast<std::uint32_t>(queue.size());
                controller_input.dropped_frames = static_cast<std::uint32_t>(queue.dropped_count());

                controller_output = rate_controller.update(controller_input);

                int initial_roi_quality = controller_output.roi_quality;
                int final_roi_quality = initial_roi_quality;
                bool reencoded = false;
                std::uint32_t reencode_attempts = 0;

                auto semantic_encode_start = std::chrono::steady_clock::now();

                SemanticEncodeResult encode_result = estimate_semantic_encode(
                    tuned_frame,
                    roi_result,
                    controller_output.roi_quality,
                    controller_output.context_quality
                );

                Frame context_frame = make_context_frame(tuned_frame, controller_output.context_width);

                PackedRoiTile roi_tile = pack_roi_tile(
                    tuned_frame,
                    roi_result,
                    controller_output.roi_cell_size,
                    controller_output.roi_cell_size,
                    controller_output.max_rois
                );

                EncodedImage context_jpeg = encode_jpeg(context_frame, encode_result.context_quality);
                EncodedImage roi_tile_jpeg = encode_jpeg(roi_tile.tile, encode_result.roi_quality);

                auto semantic_encode_end = std::chrono::steady_clock::now();
                semantic_encode_ms = elapsed_ms(semantic_encode_start, semantic_encode_end);

                std::size_t total_encoded_bytes = context_jpeg.bytes.size() + roi_tile_jpeg.bytes.size();

                double actual_bitrate_kbps = estimate_frame_bitrate_kbps(
                    total_encoded_bytes,
                    measured_fps > 0.0 ? measured_fps : source_fps
                );

                bool compressed_validation_ran_this_frame = should_run_compressed_validation(
                    maybe_frame->id,
                    detector_ran_this_frame,
                    !last_reference_detector_result.objects.empty(),
                    previous_ai_stability_loss,
                    config.confidence_loss_threshold,
                    runtime_schedule
                );

                bool semantic_packet_write_this_frame = should_write_semantic_packet(
                    maybe_frame->id,
                    detector_ran_this_frame,
                    cached_ai_stability_loss,
                    config.confidence_loss_threshold,
                    runtime_schedule
                );

                bool detection_event_write_this_frame = should_write_detection_event(
                    maybe_frame->id,
                    detector_ran_this_frame,
                    !last_event_detector_result.objects.empty(),
                    runtime_schedule
                );

                bool need_reconstruction =
                    compressed_validation_ran_this_frame ||
                    detection_event_write_this_frame ||
                    semantic_packet_write_this_frame;

                std::optional<Frame> final_reconstructed_frame;

                if (need_reconstruction) {
                    auto reconstruct_start = std::chrono::steady_clock::now();

                    Frame decoded_context = decode_jpeg(context_jpeg, tuned_frame.id);
                    Frame decoded_roi_tile = decode_jpeg(roi_tile_jpeg, tuned_frame.id);

                    PackedRoiTile decoded_packed_tile;
                    decoded_packed_tile.tile = decoded_roi_tile;
                    decoded_packed_tile.tile_cols = roi_tile.tile_cols;
                    decoded_packed_tile.tile_rows = roi_tile.tile_rows;
                    decoded_packed_tile.cell_width = roi_tile.cell_width;
                    decoded_packed_tile.cell_height = roi_tile.cell_height;

                    final_reconstructed_frame = reconstruct_semantic_frame(
                        decoded_context,
                        decoded_packed_tile,
                        roi_result,
                        tuned_frame.width,
                        tuned_frame.height
                    );

                    auto reconstruct_end = std::chrono::steady_clock::now();
                    semantic_reconstruct_ms = elapsed_ms(reconstruct_start, reconstruct_end);
                }

                StabilityResult stability = compute_roi_stability_loss(roi_tile.tile, roi_tile_jpeg);

                double reference_ai_confidence = cached_reference_ai_confidence;
                double compressed_ai_confidence = cached_compressed_ai_confidence;
                double current_detector_confidence_loss = cached_detector_confidence_loss;

                if (compressed_validation_ran_this_frame && final_reconstructed_frame.has_value()) {
                    auto validation_start = std::chrono::steady_clock::now();

                    DetectorResult raw_compressed_detector_result =
                        ai_detector.detect(*final_reconstructed_frame, roi_result);

                    last_compressed_detector_result = filter_task_relevant_objects(
                        raw_compressed_detector_result,
                        roi_filter_policy
                    );

                    compressed_ai_confidence = last_compressed_detector_result.mean_confidence;
                    current_detector_confidence_loss = detector_confidence_loss(
                        reference_ai_confidence,
                        compressed_ai_confidence
                    );

                    stability.total_loss = std::clamp(
                        0.35 * stability.total_loss + 0.65 * current_detector_confidence_loss,
                        0.0,
                        1.0
                    );

                    cached_compressed_ai_confidence = compressed_ai_confidence;
                    cached_detector_confidence_loss = current_detector_confidence_loss;
                    cached_ai_stability_loss = stability.total_loss;

                    auto validation_end = std::chrono::steady_clock::now();
                    compressed_validation_ms = elapsed_ms(validation_start, validation_end);
                } else {
                    stability.total_loss = cached_ai_stability_loss;

                    if (stability.total_loss <= 0.0) {
                        stability.total_loss = std::clamp(
                            compute_roi_stability_loss(roi_tile.tile, roi_tile_jpeg).total_loss,
                            0.0,
                            1.0
                        );
                    }

                    reference_ai_confidence = cached_reference_ai_confidence;
                    compressed_ai_confidence = cached_compressed_ai_confidence;
                    current_detector_confidence_loss = cached_detector_confidence_loss;
                }

                double initial_stability_loss = stability.total_loss;

                auto after_first_encode = std::chrono::steady_clock::now();
                double elapsed_before_reencode_ms = elapsed_ms(frame_start, after_first_encode);
                double remaining_budget_ms = config.latency_budget_ms - elapsed_before_reencode_ms;
                bool stability_high = stability.total_loss > config.confidence_loss_threshold;
                bool budget_allows_reencode = remaining_budget_ms > 8.0;

                if (compressed_validation_ran_this_frame &&
                    stability_high &&
                    controller_output.reencode_allowed &&
                    budget_allows_reencode &&
                    final_reconstructed_frame.has_value()) {
                    int repaired_quality = std::min(config.roi_quality_max, encode_result.roi_quality + 8);

                    EncodedImage repaired_roi_tile_jpeg = encode_jpeg(roi_tile.tile, repaired_quality);
                    Frame repaired_decoded_roi_tile = decode_jpeg(repaired_roi_tile_jpeg, tuned_frame.id);
                    Frame decoded_context = decode_jpeg(context_jpeg, tuned_frame.id);

                    PackedRoiTile repaired_packed_tile;
                    repaired_packed_tile.tile = repaired_decoded_roi_tile;
                    repaired_packed_tile.tile_cols = roi_tile.tile_cols;
                    repaired_packed_tile.tile_rows = roi_tile.tile_rows;
                    repaired_packed_tile.cell_width = roi_tile.cell_width;
                    repaired_packed_tile.cell_height = roi_tile.cell_height;

                    Frame repaired_reconstructed_frame = reconstruct_semantic_frame(
                        decoded_context,
                        repaired_packed_tile,
                        roi_result,
                        tuned_frame.width,
                        tuned_frame.height
                    );

                    DetectorResult raw_repaired_detector_result = ai_detector.detect(
                        repaired_reconstructed_frame,
                        roi_result
                    );

                    DetectorResult repaired_compressed_detector_result = filter_task_relevant_objects(
                        raw_repaired_detector_result,
                        roi_filter_policy
                    );

                    StabilityResult repaired_stability = compute_roi_stability_loss(
                        roi_tile.tile,
                        repaired_roi_tile_jpeg
                    );

                    double repaired_compressed_confidence =
                        repaired_compressed_detector_result.mean_confidence;

                    double repaired_detector_loss = detector_confidence_loss(
                        reference_ai_confidence,
                        repaired_compressed_confidence
                    );

                    repaired_stability.total_loss = std::clamp(
                        0.35 * repaired_stability.total_loss + 0.65 * repaired_detector_loss,
                        0.0,
                        1.0
                    );

                    if (repaired_stability.total_loss <= stability.total_loss) {
                        roi_tile_jpeg = std::move(repaired_roi_tile_jpeg);
                        total_encoded_bytes = context_jpeg.bytes.size() + roi_tile_jpeg.bytes.size();
                        stability = repaired_stability;
                        last_compressed_detector_result = repaired_compressed_detector_result;
                        compressed_ai_confidence = repaired_compressed_confidence;
                        current_detector_confidence_loss = repaired_detector_loss;
                        final_roi_quality = repaired_quality;
                        reencoded = true;
                        reencode_attempts = 1;
                        final_reconstructed_frame = repaired_reconstructed_frame;

                        cached_compressed_ai_confidence = compressed_ai_confidence;
                        cached_detector_confidence_loss = current_detector_confidence_loss;
                        cached_ai_stability_loss = stability.total_loss;
                    }
                }

                SemanticPacketInput semantic_packet_input;
                semantic_packet_input.frame_id = maybe_frame->id;
                semantic_packet_input.timestamp_ms = queue_wait_ms;
                semantic_packet_input.frame_width = static_cast<std::uint32_t>(tuned_frame.width);
                semantic_packet_input.frame_height = static_cast<std::uint32_t>(tuned_frame.height);
                semantic_packet_input.context_width = static_cast<std::uint32_t>(context_frame.width);
                semantic_packet_input.context_height = static_cast<std::uint32_t>(context_frame.height);
                semantic_packet_input.roi_tile_width = static_cast<std::uint32_t>(roi_tile.tile.width);
                semantic_packet_input.roi_tile_height = static_cast<std::uint32_t>(roi_tile.tile.height);
                semantic_packet_input.roi_quality = static_cast<std::uint32_t>(final_roi_quality);
                semantic_packet_input.context_quality = static_cast<std::uint32_t>(encode_result.context_quality);
                semantic_packet_input.detector_interval = static_cast<std::uint32_t>(controller_output.detector_interval);
                semantic_packet_input.controller_state = controller_output.controller_state;
                semantic_packet_input.reference_ai_confidence = reference_ai_confidence;
                semantic_packet_input.compressed_ai_confidence = compressed_ai_confidence;
                semantic_packet_input.detector_confidence_loss = current_detector_confidence_loss;
                semantic_packet_input.ai_stability_loss = stability.total_loss;
                semantic_packet_input.roi_result = roi_result;
                semantic_packet_input.detector_result = last_reference_detector_result;
                semantic_packet_input.context_jpeg = context_jpeg;
                semantic_packet_input.roi_tile_jpeg = roi_tile_jpeg;

                if (semantic_packet_write_this_frame) {
                    write_semantic_packet(semantic_packet_input, output_dir);
                }

                if ((semantic_packet_write_this_frame || compressed_validation_ran_this_frame) &&
                    final_reconstructed_frame.has_value()) {
                    std::string validation_type =
                        compressed_validation_ran_this_frame ? "ai_validation" : "semantic_snapshot";

                    write_validation_snapshot(
                        tuned_frame,
                        *final_reconstructed_frame,
                        maybe_frame->id,
                        validation_type,
                        reference_ai_confidence,
                        compressed_ai_confidence,
                        current_detector_confidence_loss,
                        stability.total_loss,
                        compressed_validation_ran_this_frame,
                        detector_ran_this_frame,
                        output_dir
                    );
                }

                previous_bitrate_kbps = actual_bitrate_kbps;
                previous_ai_stability_loss = stability.total_loss;

                MetadataPacketInput metadata_input;
                metadata_input.frame_id = maybe_frame->id;
                metadata_input.latency_ms = queue_wait_ms;
                metadata_input.bitrate_kbps = actual_bitrate_kbps;
                metadata_input.ai_stability_loss = stability.total_loss;
                metadata_input.context_width = context_frame.width;
                metadata_input.context_height = context_frame.height;
                metadata_input.roi_tile_width = roi_tile.tile.width;
                metadata_input.roi_tile_height = roi_tile.tile.height;
                metadata_input.roi_quality = final_roi_quality;
                metadata_input.context_quality = encode_result.context_quality;
                metadata_input.tile_cols = roi_tile.tile_cols;
                metadata_input.tile_rows = roi_tile.tile_rows;
                metadata_input.cell_width = roi_tile.cell_width;
                metadata_input.cell_height = roi_tile.cell_height;
                metadata_input.reencoded = reencoded;
                metadata_input.controller_state = controller_output.controller_state;
                metadata_input.roi_result = roi_result;

                std::string metadata_json = metadata_to_json(metadata_input);

                if (detection_event_write_this_frame && final_reconstructed_frame.has_value()) {
                    auto event_start = std::chrono::steady_clock::now();

                    write_detection_event_snapshot(
                        tuned_frame,
                        *final_reconstructed_frame,
                        last_event_detector_result,
                        maybe_frame->id,
                        reference_ai_confidence,
                        compressed_ai_confidence,
                        current_detector_confidence_loss,
                        stability.total_loss,
                        output_dir
                    );

                    auto event_end = std::chrono::steady_clock::now();
                    event_write_ms = elapsed_ms(event_start, event_end);
                }

                auto frame_done = std::chrono::steady_clock::now();
                double processing_ms = elapsed_ms(frame_start, frame_done);
                double latency_ms = elapsed_ms(maybe_frame->created_at, frame_done);

                EngineMetrics metrics;
                metrics.frame_id = maybe_frame->id;
                metrics.fps = measured_fps;
                metrics.latency_ms = latency_ms;
                metrics.bitrate_kbps = actual_bitrate_kbps;

                metrics.queue_wait_ms = queue_wait_ms;
                metrics.processing_ms = processing_ms;
                metrics.isp_ms = isp_ms;
                metrics.motion_roi_ms = motion_roi_ms;
                metrics.detector_ms = detector_ms;
                metrics.semantic_encode_ms = semantic_encode_ms;
                metrics.semantic_reconstruct_ms = semantic_reconstruct_ms;
                metrics.compressed_validation_ms = compressed_validation_ms;
                metrics.event_write_ms = event_write_ms;

                metrics.input_brightness = input_stats.mean_brightness;
                metrics.input_contrast = input_stats.contrast;
                metrics.input_sharpness = input_stats.sharpness;
                metrics.input_noise = input_stats.noise;
                metrics.mean_brightness = output_stats.mean_brightness;
                metrics.output_contrast = output_stats.contrast;
                metrics.output_sharpness = output_stats.sharpness;
                metrics.output_noise = output_stats.noise;
                metrics.brightness_gain = isp_settings.brightness_gain;
                metrics.gamma = isp_settings.gamma;
                metrics.contrast_alpha = isp_settings.contrast_alpha;
                metrics.contrast_beta = isp_settings.contrast_beta;
                metrics.denoise_strength = isp_settings.denoise_strength;
                metrics.sharpen_amount = isp_settings.sharpen_amount;
                metrics.clahe_enabled = isp_settings.clahe_enabled;
                metrics.isp_profile = isp_settings.profile;
                metrics.roi_count = static_cast<std::uint32_t>(roi_result.boxes.size());
                metrics.roi_area_ratio = current_roi_area_ratio;
                metrics.detected_object_count = static_cast<std::uint32_t>(last_reference_detector_result.objects.size());
                metrics.detector_ran = detector_ran_this_frame;
                metrics.detector_used_dnn = last_reference_detector_result.used_dnn;
                metrics.raw_detector_candidates = static_cast<std::uint32_t>(last_reference_detector_result.raw_candidate_count);
                metrics.max_detector_confidence = last_reference_detector_result.max_raw_confidence;
                metrics.reference_ai_confidence = reference_ai_confidence;
                metrics.compressed_ai_confidence = compressed_ai_confidence;
                metrics.detector_confidence_loss = current_detector_confidence_loss;
                metrics.roi_quality = static_cast<std::uint32_t>(final_roi_quality);
                metrics.context_quality = static_cast<std::uint32_t>(encode_result.context_quality);
                metrics.context_width = static_cast<std::uint32_t>(context_frame.width);
                metrics.context_height = static_cast<std::uint32_t>(context_frame.height);
                metrics.roi_tile_width = static_cast<std::uint32_t>(roi_tile.tile.width);
                metrics.roi_tile_height = static_cast<std::uint32_t>(roi_tile.tile.height);
                metrics.context_jpeg_bytes = static_cast<std::uint32_t>(context_jpeg.bytes.size());
                metrics.roi_tile_jpeg_bytes = static_cast<std::uint32_t>(roi_tile_jpeg.bytes.size());
                metrics.total_encoded_bytes = static_cast<std::uint32_t>(total_encoded_bytes);
                metrics.detector_interval = static_cast<std::uint32_t>(effective_detector_interval);
                metrics.reencode_allowed = controller_output.reencode_allowed;
                metrics.reencoded = reencoded;
                metrics.reencode_attempts = reencode_attempts;
                metrics.initial_roi_quality = static_cast<std::uint32_t>(initial_roi_quality);
                metrics.final_roi_quality = static_cast<std::uint32_t>(final_roi_quality);
                metrics.initial_ai_stability_loss = initial_stability_loss;
                metrics.controller_state = controller_output.controller_state;
                metrics.controller_mode = controller_output.controller_mode;
                metrics.controller_reason = controller_output.controller_reason;
                metrics.controller_action = controller_output.controller_action;
                metrics.ai_stability_loss = stability.total_loss;
                metrics.cpu_percent = process_monitor.cpu_percent();
                metrics.ram_mb = process_monitor.ram_mb();
                metrics.dropped_frames = static_cast<std::uint32_t>(queue.dropped_count());
                metrics.queue_depth = static_cast<std::uint32_t>(queue.size());

                if (last_reference_detector_result.used_dnn || last_compressed_detector_result.used_dnn) {
                    metrics.mode = "dnn_detector";
                } else {
                    metrics.mode = "motion_detector_fallback";
                }

                std::string metrics_json = metrics_to_json(metrics);

                std::cout << metrics_json << std::endl;
                metrics_log.write_line(metrics_json);
                metadata_log.write_line(metadata_json);
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(40));
        }
    });

    producer.join();
    consumer.join();

    return 0;
}