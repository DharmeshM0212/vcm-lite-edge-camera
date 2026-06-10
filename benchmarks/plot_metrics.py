from pathlib import Path

import matplotlib.pyplot as plt
import pandas as pd


def ensure_output_dir(output_dir: str | Path) -> Path:
    path = Path(output_dir)
    path.mkdir(parents=True, exist_ok=True)
    return path


def numeric_series(df: pd.DataFrame, column: str) -> pd.Series:
    if column not in df.columns:
        return pd.Series(dtype=float)

    return pd.to_numeric(df[column], errors="coerce").fillna(0.0)


def frame_axis(df: pd.DataFrame) -> pd.Series:
    if "frame_id" in df.columns:
        return numeric_series(df, "frame_id")

    return pd.Series(range(len(df)))


def task_loss_name(df: pd.DataFrame) -> str:
    if "task_preservation_loss" in df.columns:
        return "task_preservation_loss"

    return "ai_stability_loss"


def plot_line(df: pd.DataFrame, y_column: str, title: str, ylabel: str, output_path: Path) -> None:
    if df.empty or y_column not in df.columns:
        return

    x = frame_axis(df)
    y = numeric_series(df, y_column)

    plt.figure(figsize=(10, 4.8))
    plt.plot(x, y)
    plt.xlabel("Frame ID")
    plt.ylabel(ylabel)
    plt.title(title)
    plt.tight_layout()
    plt.savefig(output_path, dpi=150)
    plt.close()


def plot_qualities(df: pd.DataFrame, output_path: Path) -> None:
    if df.empty:
        return

    x = frame_axis(df)

    plt.figure(figsize=(10, 4.8))

    if "roi_quality" in df.columns:
        plt.plot(x, numeric_series(df, "roi_quality"), label="Object ROI quality")

    if "context_quality" in df.columns:
        plt.plot(x, numeric_series(df, "context_quality"), label="Background quality")

    plt.xlabel("Frame ID")
    plt.ylabel("JPEG quality")
    plt.title("Adaptive Quality Decisions")
    plt.legend()
    plt.tight_layout()
    plt.savefig(output_path, dpi=150)
    plt.close()


def plot_controller_states(df: pd.DataFrame, output_path: Path) -> None:
    if df.empty or "controller_state" not in df.columns:
        return

    counts = df["controller_state"].astype(str).value_counts()

    plt.figure(figsize=(10, 4.8))
    plt.bar(counts.index, counts.values)
    plt.xlabel("Controller state")
    plt.ylabel("Frame count")
    plt.title("Controller State Distribution")
    plt.xticks(rotation=30, ha="right")
    plt.tight_layout()
    plt.savefig(output_path, dpi=150)
    plt.close()


def plot_timing_breakdown(df: pd.DataFrame, output_path: Path) -> None:
    if df.empty:
        return

    columns = [
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

    labels = []
    values = []

    for column in columns:
        if column in df.columns:
            labels.append(column.replace("_ms", "").replace("_", " "))
            values.append(float(numeric_series(df, column).mean()))

    if not labels:
        return

    plt.figure(figsize=(10, 5.2))
    plt.bar(labels, values)
    plt.xlabel("Pipeline stage")
    plt.ylabel("Average time (ms)")
    plt.title("Average Runtime Breakdown")
    plt.xticks(rotation=35, ha="right")
    plt.tight_layout()
    plt.savefig(output_path, dpi=150)
    plt.close()


def plot_confidence(df: pd.DataFrame, output_path: Path) -> None:
    if df.empty:
        return

    x = frame_axis(df)

    plt.figure(figsize=(10, 4.8))

    if "reference_ai_confidence" in df.columns:
        plt.plot(x, numeric_series(df, "reference_ai_confidence"), label="Reference confidence")

    if "compressed_ai_confidence" in df.columns:
        plt.plot(x, numeric_series(df, "compressed_ai_confidence"), label="Reconstructed confidence")

    plt.xlabel("Frame ID")
    plt.ylabel("Detector confidence")
    plt.title("Task Confidence Before and After Semantic Reconstruction")
    plt.legend()
    plt.tight_layout()
    plt.savefig(output_path, dpi=150)
    plt.close()


def plot_all(df: pd.DataFrame, output_dir: str | Path) -> None:
    output_path = ensure_output_dir(output_dir)
    loss_column = task_loss_name(df)

    plot_line(
        df,
        "latency_ms",
        "Frame Latency",
        "Latency (ms)",
        output_path / "latency_ms.png"
    )

    plot_line(
        df,
        "fps",
        "Measured FPS",
        "FPS",
        output_path / "fps.png"
    )

    plot_line(
        df,
        "bitrate_kbps",
        "Semantic Packet Bitrate",
        "Bitrate (kbps)",
        output_path / "bitrate_kbps.png"
    )

    plot_line(
        df,
        loss_column,
        "Task Preservation Loss",
        "Loss",
        output_path / "task_preservation_loss.png"
    )

    plot_line(
        df,
        "detector_confidence_loss",
        "Detection Confidence Drop",
        "Confidence drop",
        output_path / "detector_confidence_drop.png"
    )

    plot_line(
        df,
        "semantic_psnr_db",
        "Semantic Reconstruction PSNR",
        "PSNR (dB)",
        output_path / "semantic_psnr_db.png"
    )

    plot_line(
        df,
        "semantic_ssim",
        "Semantic Reconstruction SSIM",
        "SSIM",
        output_path / "semantic_ssim.png"
    )

    plot_line(
        df,
        "roi_area_ratio",
        "Object ROI Area Ratio",
        "ROI area ratio",
        output_path / "roi_area_ratio.png"
    )

    plot_confidence(df, output_path / "task_confidence.png")
    plot_qualities(df, output_path / "qualities.png")
    plot_controller_states(df, output_path / "controller_states.png")
    plot_timing_breakdown(df, output_path / "timing_breakdown.png")