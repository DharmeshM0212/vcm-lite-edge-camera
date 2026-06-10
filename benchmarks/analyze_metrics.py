import json
from collections import Counter
from pathlib import Path
from typing import Any

import pandas as pd


def load_metrics_jsonl(path: str | Path) -> pd.DataFrame:
    rows: list[dict[str, Any]] = []
    file_path = Path(path)

    if not file_path.exists():
        raise FileNotFoundError(str(file_path))

    with file_path.open("r", encoding="utf-8") as file:
        for line in file:
            stripped = line.strip()
            if not stripped:
                continue

            try:
                rows.append(json.loads(stripped))
            except json.JSONDecodeError:
                continue

    if not rows:
        return pd.DataFrame()

    return pd.DataFrame(rows)


def numeric_series(df: pd.DataFrame, column: str) -> pd.Series:
    if column not in df.columns or df.empty:
        return pd.Series(dtype=float)

    return pd.to_numeric(df[column], errors="coerce").fillna(0.0)


def safe_mean(df: pd.DataFrame, column: str) -> float:
    series = numeric_series(df, column)

    if series.empty:
        return 0.0

    return float(series.mean())


def safe_mean_positive(df: pd.DataFrame, column: str) -> float:
    series = numeric_series(df, column)
    series = series[series > 0.0]

    if series.empty:
        return 0.0

    return float(series.mean())


def safe_percentile(df: pd.DataFrame, column: str, q: float) -> float:
    series = numeric_series(df, column)

    if series.empty:
        return 0.0

    return float(series.quantile(q))


def safe_percentile_positive(df: pd.DataFrame, column: str, q: float) -> float:
    series = numeric_series(df, column)
    series = series[series > 0.0]

    if series.empty:
        return 0.0

    return float(series.quantile(q))


def safe_max(df: pd.DataFrame, column: str) -> float:
    series = numeric_series(df, column)

    if series.empty:
        return 0.0

    return float(series.max())


def count_true(df: pd.DataFrame, column: str) -> int:
    if column not in df.columns or df.empty:
        return 0

    return int(df[column].astype(bool).sum())


def task_loss_column(df: pd.DataFrame) -> str:
    if "task_preservation_loss" in df.columns:
        return "task_preservation_loss"

    return "ai_stability_loss"


def timing_summary(df: pd.DataFrame, columns: list[str]) -> dict[str, Any]:
    output = {}

    for column in columns:
        output[column] = {
            "avg": safe_mean(df, column),
            "p95": safe_percentile(df, column, 0.95),
            "max": safe_max(df, column),
        }

    return output


def summarize_metrics(df: pd.DataFrame) -> dict[str, Any]:
    if df.empty:
        return {"frames": 0}

    loss_column = task_loss_column(df)

    controller_counts = {}
    mode_counts = {}

    if "controller_state" in df.columns:
        controller_counts = dict(Counter(df["controller_state"].astype(str).tolist()))

    if "mode" in df.columns:
        mode_counts = dict(Counter(df["mode"].astype(str).tolist()))

    timing_columns = [
        "queue_wait_ms",
        "processing_ms",
        "isp_ms",
        "motion_roi_ms",
        "detector_ms",
        "semantic_encode_ms",
        "semantic_reconstruct_ms",
        "compressed_validation_ms",
        "event_write_ms",
    ]

    summary = {
        "frames": int(len(df)),
        "first_frame_id": int(df["frame_id"].iloc[0]) if "frame_id" in df.columns else 0,
        "last_frame_id": int(df["frame_id"].iloc[-1]) if "frame_id" in df.columns else 0,
        "average_fps": safe_mean(df, "fps"),
        "p95_fps": safe_percentile(df, "fps", 0.95),
        "average_latency_ms": safe_mean(df, "latency_ms"),
        "p95_latency_ms": safe_percentile(df, "latency_ms", 0.95),
        "max_latency_ms": safe_max(df, "latency_ms"),
        "average_bitrate_kbps": safe_mean(df, "bitrate_kbps"),
        "p95_bitrate_kbps": safe_percentile(df, "bitrate_kbps", 0.95),
        "max_bitrate_kbps": safe_max(df, "bitrate_kbps"),
        "average_task_preservation_loss": safe_mean(df, loss_column),
        "p95_task_preservation_loss": safe_percentile(df, loss_column, 0.95),
        "average_ai_stability_loss": safe_mean(df, "ai_stability_loss"),
        "average_semantic_psnr_db": safe_mean_positive(df, "semantic_psnr_db"),
        "p95_semantic_psnr_db": safe_percentile_positive(df, "semantic_psnr_db", 0.95),
        "average_semantic_ssim": safe_mean_positive(df, "semantic_ssim"),
        "p95_semantic_ssim": safe_percentile_positive(df, "semantic_ssim", 0.95),
        "average_detector_confidence_loss": safe_mean(df, "detector_confidence_loss"),
        "average_reference_ai_confidence": safe_mean(df, "reference_ai_confidence"),
        "average_reconstructed_ai_confidence": safe_mean(df, "compressed_ai_confidence"),
        "average_roi_count": safe_mean(df, "roi_count"),
        "average_roi_area_ratio": safe_mean(df, "roi_area_ratio"),
        "p95_roi_area_ratio": safe_percentile(df, "roi_area_ratio", 0.95),
        "average_roi_quality": safe_mean(df, "roi_quality"),
        "average_context_quality": safe_mean(df, "context_quality"),
        "average_context_width": safe_mean(df, "context_width"),
        "average_roi_tile_width": safe_mean(df, "roi_tile_width"),
        "average_total_encoded_bytes": safe_mean(df, "total_encoded_bytes"),
        "final_dropped_frames": int(safe_max(df, "dropped_frames")),
        "max_queue_depth": int(safe_max(df, "queue_depth")),
        "reencoded_frames": count_true(df, "reencoded"),
        "detector_ran_frames": count_true(df, "detector_ran"),
        "dnn_frames": count_true(df, "detector_used_dnn"),
        "controller_state_counts": controller_counts,
        "mode_counts": mode_counts,
        "timing_ms": timing_summary(df, timing_columns),
    }

    return summary


def write_summary(summary: dict[str, Any], output_dir: str | Path) -> None:
    output_path = Path(output_dir)
    output_path.mkdir(parents=True, exist_ok=True)

    json_path = output_path / "summary.json"
    csv_path = output_path / "summary.csv"

    json_path.write_text(json.dumps(summary, indent=2), encoding="utf-8")

    flat_summary = {}

    for key, value in summary.items():
        if isinstance(value, dict):
            for inner_key, inner_value in value.items():
                if isinstance(inner_value, dict):
                    for nested_key, nested_value in inner_value.items():
                        flat_summary[f"{key}.{inner_key}.{nested_key}"] = nested_value
                else:
                    flat_summary[f"{key}.{inner_key}"] = inner_value
        else:
            flat_summary[key] = value

    pd.DataFrame([flat_summary]).to_csv(csv_path, index=False)