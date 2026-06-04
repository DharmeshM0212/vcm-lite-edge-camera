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

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>
#include <thread>

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
        ProcessMonitor process_monitor;

        DetectorResult last_reference_detector_result;
        last_reference_detector_result.mean_confidence = 0.0;
        last_reference_detector_result.used_dnn = false;
        last_reference_detector_result.detector_ran = false;
        last_reference_detector_result.raw_candidate_count = 0;
        last_reference_detector_result.max_raw_confidence = 0.0;

        DetectorResult last_compressed_detector_result;
        last_compressed_detector_result.mean_confidence = 0.0;
        last_compressed_detector_result.used_dnn = false;
        last_compressed_detector_result.detector_ran = false;
        last_compressed_detector_result.raw_candidate_count = 0;
        last_compressed_detector_result.max_raw_confidence = 0.0;

        std::uint64_t last_detector_frame_id = 0;
        bool has_detector_frame = false;
        bool detector_ran_this_frame = false;

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
                double latency_ms = std::chrono::duration<double, std::milli>(frame_start - maybe_frame->created_at).count();

                ImageStats input_stats = compute_image_stats(*maybe_frame);
                IspSettings isp_settings = choose_isp_settings(input_stats);
                Frame tuned_frame = apply_isp_tuning(*maybe_frame, isp_settings);
                ImageStats output_stats = compute_image_stats(tuned_frame);

                RoiResult motion_roi_result = motion_detector.detect(tuned_frame);

                bool warmup_detector = maybe_frame->id < 30;
                bool scheduled_detector = maybe_frame->id % static_cast<std::uint64_t>(std::max(1, controller_output.detector_interval)) == 0;
                bool force_detector_on_ai_loss = previous_ai_stability_loss > 0.75 * config.confidence_loss_threshold;
                bool run_detector = warmup_detector || scheduled_detector || force_detector_on_ai_loss;

                detector_ran_this_frame = run_detector;

                if (run_detector) {
                    last_reference_detector_result = ai_detector.detect(tuned_frame, motion_roi_result);
                    last_detector_frame_id = maybe_frame->id;
                    has_detector_frame = true;
                }

                std::uint64_t detector_age = has_detector_frame ? maybe_frame->id - last_detector_frame_id : 999999;
                bool object_roi_fresh = has_detector_frame && detector_age <= 5 && !last_reference_detector_result.objects.empty();

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
                controller_input.latency_ms = latency_ms;
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

                Frame decoded_context = decode_jpeg(context_jpeg, tuned_frame.id);
                Frame decoded_roi_tile = decode_jpeg(roi_tile_jpeg, tuned_frame.id);

                PackedRoiTile decoded_packed_tile;
                decoded_packed_tile.tile = decoded_roi_tile;
                decoded_packed_tile.tile_cols = roi_tile.tile_cols;
                decoded_packed_tile.tile_rows = roi_tile.tile_rows;
                decoded_packed_tile.cell_width = roi_tile.cell_width;
                decoded_packed_tile.cell_height = roi_tile.cell_height;

                Frame reconstructed_frame = reconstruct_semantic_frame(
                    decoded_context,
                    decoded_packed_tile,
                    roi_result,
                    tuned_frame.width,
                    tuned_frame.height
                );
                Frame final_decoded_roi_tile = decoded_roi_tile;
                Frame final_reconstructed_frame = reconstructed_frame;

                if (run_detector) {
                    last_compressed_detector_result = ai_detector.detect(reconstructed_frame, roi_result);
                }

                StabilityResult stability = compute_roi_stability_loss(roi_tile.tile, roi_tile_jpeg);
                double reference_ai_confidence = last_reference_detector_result.mean_confidence;
                double compressed_ai_confidence = last_compressed_detector_result.mean_confidence;
                double current_detector_confidence_loss = detector_confidence_loss(reference_ai_confidence, compressed_ai_confidence);

                if (last_reference_detector_result.used_dnn || last_compressed_detector_result.used_dnn) {
                    stability.total_loss = std::clamp(0.35 * stability.total_loss + 0.65 * current_detector_confidence_loss, 0.0, 1.0);
                } else {
                    stability.total_loss = std::clamp(stability.total_loss, 0.0, 1.0);
                    compressed_ai_confidence = std::clamp(reference_ai_confidence * (1.0 - stability.total_loss), 0.0, 1.0);
                    current_detector_confidence_loss = detector_confidence_loss(reference_ai_confidence, compressed_ai_confidence);
                }

                double initial_stability_loss = stability.total_loss;

                auto after_first_encode = std::chrono::steady_clock::now();
                double elapsed_ms = std::chrono::duration<double, std::milli>(after_first_encode - frame_start).count();
                double remaining_budget_ms = config.latency_budget_ms - elapsed_ms;
                bool stability_high = stability.total_loss > config.confidence_loss_threshold;
                bool budget_allows_reencode = remaining_budget_ms > 8.0;

                if (stability_high && controller_output.reencode_allowed && budget_allows_reencode) {
                    int repaired_quality = std::min(config.roi_quality_max, encode_result.roi_quality + 8);

                    EncodedImage repaired_roi_tile_jpeg = encode_jpeg(roi_tile.tile, repaired_quality);
                    Frame repaired_decoded_roi_tile = decode_jpeg(repaired_roi_tile_jpeg, tuned_frame.id);

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
                    final_decoded_roi_tile = repaired_decoded_roi_tile;
                    final_reconstructed_frame = repaired_reconstructed_frame;

                    DetectorResult repaired_compressed_detector_result = last_compressed_detector_result;

                    if (run_detector) {
                        repaired_compressed_detector_result = ai_detector.detect(repaired_reconstructed_frame, roi_result);
                    }

                    StabilityResult repaired_stability = compute_roi_stability_loss(roi_tile.tile, repaired_roi_tile_jpeg);
                    double repaired_compressed_confidence = repaired_compressed_detector_result.mean_confidence;
                    double repaired_detector_loss = detector_confidence_loss(reference_ai_confidence, repaired_compressed_confidence);

                    if (last_reference_detector_result.used_dnn || repaired_compressed_detector_result.used_dnn) {
                        repaired_stability.total_loss = std::clamp(0.35 * repaired_stability.total_loss + 0.65 * repaired_detector_loss, 0.0, 1.0);
                    } else {
                        repaired_stability.total_loss = std::clamp(repaired_stability.total_loss, 0.0, 1.0);
                        repaired_compressed_confidence = std::clamp(reference_ai_confidence * (1.0 - repaired_stability.total_loss), 0.0, 1.0);
                        repaired_detector_loss = detector_confidence_loss(reference_ai_confidence, repaired_compressed_confidence);
                    }

                    if (repaired_stability.total_loss <= stability.total_loss) {
                        roi_tile_jpeg = std::move(repaired_roi_tile_jpeg);
                        stability = repaired_stability;
                        last_compressed_detector_result = repaired_compressed_detector_result;
                        compressed_ai_confidence = repaired_compressed_confidence;
                        current_detector_confidence_loss = repaired_detector_loss;
                        final_roi_quality = repaired_quality;
                        reencoded = true;
                        reencode_attempts = 1;
                    }
                }

                std::size_t total_encoded_bytes = context_jpeg.bytes.size() + roi_tile_jpeg.bytes.size();
                double actual_bitrate_kbps = estimate_frame_bitrate_kbps(
                    total_encoded_bytes,
                    measured_fps > 0.0 ? measured_fps : source_fps
                );
                SemanticPacketInput semantic_packet_input;
                semantic_packet_input.frame_id = maybe_frame->id;
                semantic_packet_input.timestamp_ms = latency_ms;
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

                write_semantic_packet(semantic_packet_input, output_dir);

                previous_bitrate_kbps = actual_bitrate_kbps;
                previous_ai_stability_loss = stability.total_loss;

                auto frame_done = std::chrono::steady_clock::now();
                latency_ms = std::chrono::duration<double, std::milli>(frame_done - maybe_frame->created_at).count();

                MetadataPacketInput metadata_input;
                metadata_input.frame_id = maybe_frame->id;
                metadata_input.latency_ms = latency_ms;
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

                EngineMetrics metrics;
                metrics.frame_id = maybe_frame->id;
                metrics.fps = measured_fps;
                metrics.latency_ms = latency_ms;
                metrics.bitrate_kbps = actual_bitrate_kbps;
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
                metrics.detector_interval = static_cast<std::uint32_t>(controller_output.detector_interval);
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

                if (detector_ran_this_frame && !last_reference_detector_result.objects.empty()) {
                    write_detection_event_snapshot(
                        tuned_frame,
                        final_reconstructed_frame,
                        last_reference_detector_result,
                        maybe_frame->id,
                        reference_ai_confidence,
                        compressed_ai_confidence,
                        current_detector_confidence_loss,
                        stability.total_loss,
                        output_dir
                    );
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