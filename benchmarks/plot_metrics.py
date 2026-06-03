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
        plt.plot(x, numeric_series(df, "roi_quality"), label="ROI quality")

    if "context_quality" in df.columns:
        plt.plot(x, numeric_series(df, "context_quality"), label="Context quality")

    plt.xlabel("Frame ID")
    plt.ylabel("Quality")
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
    plt.xlabel("Controller State")
    plt.ylabel("Frame Count")
    plt.title("Controller State Distribution")
    plt.xticks(rotation=30, ha="right")
    plt.tight_layout()
    plt.savefig(output_path, dpi=150)
    plt.close()


def plot_all(df: pd.DataFrame, output_dir: str | Path) -> None:
    output_path = ensure_output_dir(output_dir)

    plot_line(
        df,
        "latency_ms",
        "Frame Latency",
        "Latency (ms)",
        output_path / "latency_ms.png"
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
        "ai_stability_loss",
        "AI Stability Loss",
        "Loss",
        output_path / "ai_stability_loss.png"
    )

    plot_line(
        df,
        "detector_confidence_loss",
        "Detector Confidence Loss",
        "Loss",
        output_path / "detector_confidence_loss.png"
    )

    plot_line(
        df,
        "fps",
        "Measured FPS",
        "FPS",
        output_path / "fps.png"
    )

    plot_qualities(df, output_path / "qualities.png")
    plot_controller_states(df, output_path / "controller_states.png")