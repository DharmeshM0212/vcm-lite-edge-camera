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


def safe_mean(df: pd.DataFrame, column: str) -> float:
    if column not in df.columns or df.empty:
        return 0.0
    return float(pd.to_numeric(df[column], errors="coerce").fillna(0.0).mean())


def safe_percentile(df: pd.DataFrame, column: str, q: float) -> float:
    if column not in df.columns or df.empty:
        return 0.0
    return float(pd.to_numeric(df[column], errors="coerce").fillna(0.0).quantile(q))


def safe_max(df: pd.DataFrame, column: str) -> float:
    if column not in df.columns or df.empty:
        return 0.0
    return float(pd.to_numeric(df[column], errors="coerce").fillna(0.0).max())


def count_true(df: pd.DataFrame, column: str) -> int:
    if column not in df.columns or df.empty:
        return 0
    return int(df[column].astype(bool).sum())


def summarize_metrics(df: pd.DataFrame) -> dict[str, Any]:
    if df.empty:
        return {"frames": 0}

    controller_counts = {}

    if "controller_state" in df.columns:
        controller_counts = dict(Counter(df["controller_state"].astype(str).tolist()))

    summary = {
        "frames": int(len(df)),
        "first_frame_id": int(df["frame_id"].iloc[0]) if "frame_id" in df.columns else 0,
        "last_frame_id": int(df["frame_id"].iloc[-1]) if "frame_id" in df.columns else 0,
        "average_fps": safe_mean(df, "fps"),
        "average_latency_ms": safe_mean(df, "latency_ms"),
        "p95_latency_ms": safe_percentile(df, "latency_ms", 0.95),
        "max_latency_ms": safe_max(df, "latency_ms"),
        "average_bitrate_kbps": safe_mean(df, "bitrate_kbps"),
        "p95_bitrate_kbps": safe_percentile(df, "bitrate_kbps", 0.95),
        "average_ai_stability_loss": safe_mean(df, "ai_stability_loss"),
        "p95_ai_stability_loss": safe_percentile(df, "ai_stability_loss", 0.95),
        "average_detector_confidence_loss": safe_mean(df, "detector_confidence_loss"),
        "average_reference_ai_confidence": safe_mean(df, "reference_ai_confidence"),
        "average_compressed_ai_confidence": safe_mean(df, "compressed_ai_confidence"),
        "average_roi_count": safe_mean(df, "roi_count"),
        "average_roi_area_ratio": safe_mean(df, "roi_area_ratio"),
        "average_roi_quality": safe_mean(df, "roi_quality"),
        "average_context_quality": safe_mean(df, "context_quality"),
        "average_context_width": safe_mean(df, "context_width"),
        "average_roi_tile_width": safe_mean(df, "roi_tile_width"),
        "final_dropped_frames": int(safe_max(df, "dropped_frames")),
        "max_queue_depth": int(safe_max(df, "queue_depth")),
        "reencoded_frames": count_true(df, "reencoded"),
        "detector_ran_frames": count_true(df, "detector_ran"),
        "dnn_frames": count_true(df, "detector_used_dnn"),
        "controller_state_counts": controller_counts,
    }

    if "mode" in df.columns:
        summary["mode_counts"] = dict(Counter(df["mode"].astype(str).tolist()))

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
                flat_summary[f"{key}.{inner_key}"] = inner_value
        else:
            flat_summary[key] = value

    pd.DataFrame([flat_summary]).to_csv(csv_path, index=False)