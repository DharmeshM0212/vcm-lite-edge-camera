import argparse
from pathlib import Path

import matplotlib.pyplot as plt
import pandas as pd

from analyze_metrics import load_metrics_jsonl, summarize_metrics, write_summary
from plot_metrics import plot_all


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_METRICS_LOG = ROOT / "logs" / "metrics.jsonl"
DEFAULT_OUTPUT_DIR = ROOT / "benchmarks" / "results"
DEFAULT_FINAL_RUNS_DIR = ROOT / "benchmarks" / "final_runs"


def flatten_summary(summary: dict) -> dict:
    output = {}

    for key, value in summary.items():
        if isinstance(value, dict):
            for inner_key, inner_value in value.items():
                if isinstance(inner_value, dict):
                    for nested_key, nested_value in inner_value.items():
                        output[f"{key}.{inner_key}.{nested_key}"] = nested_value
                else:
                    output[f"{key}.{inner_key}"] = inner_value
        else:
            output[key] = value

    return output


def print_summary(summary: dict, label: str) -> None:
    print("")
    print("scenario:", label)
    print("frames:", summary.get("frames", 0))
    print("average_fps:", round(summary.get("average_fps", 0.0), 3))
    print("average_latency_ms:", round(summary.get("average_latency_ms", 0.0), 3))
    print("p95_latency_ms:", round(summary.get("p95_latency_ms", 0.0), 3))
    print("average_bitrate_kbps:", round(summary.get("average_bitrate_kbps", 0.0), 3))
    print("average_task_preservation_loss:", round(summary.get("average_task_preservation_loss", 0.0), 4))
    print("average_semantic_psnr_db:", round(summary.get("average_semantic_psnr_db", 0.0), 3))
    print("average_semantic_ssim:", round(summary.get("average_semantic_ssim", 0.0), 4))
    print("average_roi_area_ratio:", round(summary.get("average_roi_area_ratio", 0.0), 4))

    print("average_input_brightness:", round(summary.get("average_input_brightness", 0.0), 3))
    print("average_output_brightness:", round(summary.get("average_output_brightness", 0.0), 3))
    print("average_input_contrast:", round(summary.get("average_input_contrast", 0.0), 3))
    print("average_output_contrast:", round(summary.get("average_output_contrast", 0.0), 3))
    print("average_brightness_gain:", round(summary.get("average_brightness_gain", 0.0), 3))
    print("average_gamma:", round(summary.get("average_gamma", 0.0), 3))
    print("average_denoise_strength:", round(summary.get("average_denoise_strength", 0.0), 3))
    print("average_sharpen_amount:", round(summary.get("average_sharpen_amount", 0.0), 3))
    print("clahe_enabled_ratio:", round(summary.get("clahe_enabled_ratio", 0.0), 3))
    print("average_isp_ms:", round(summary.get("average_isp_ms", 0.0), 3))
    print("p95_isp_ms:", round(summary.get("p95_isp_ms", 0.0), 3))
    print("isp_profile_counts:", summary.get("isp_profile_counts", {}))

    print("controller_state_counts:", summary.get("controller_state_counts", {}))


def analyze_one(metrics_path: Path, output_dir: Path, label: str) -> dict:
    df = load_metrics_jsonl(metrics_path)
    summary = summarize_metrics(df)

    output_dir.mkdir(parents=True, exist_ok=True)

    write_summary(summary, output_dir)
    plot_all(df, output_dir)
    print_summary(summary, label)

    return summary


def plot_final_comparison(comparison_df: pd.DataFrame, output_dir: Path) -> None:
    if comparison_df.empty:
        return

    output_dir.mkdir(parents=True, exist_ok=True)

    metric_specs = [
        ("average_latency_ms", "Average Latency by Scenario", "Latency (ms)", "comparison_latency_ms.png"),
        ("p95_latency_ms", "P95 Latency by Scenario", "Latency (ms)", "comparison_p95_latency_ms.png"),
        ("average_bitrate_kbps", "Average Semantic Bitrate by Scenario", "Bitrate (kbps)", "comparison_bitrate_kbps.png"),
        ("average_task_preservation_loss", "Average Task Preservation Loss by Scenario", "Loss", "comparison_task_preservation_loss.png"),
        ("average_semantic_psnr_db", "Average Semantic PSNR by Scenario", "PSNR (dB)", "comparison_semantic_psnr_db.png"),
        ("average_semantic_ssim", "Average Semantic SSIM by Scenario", "SSIM", "comparison_semantic_ssim.png"),
        ("average_roi_area_ratio", "Average ROI Area Ratio by Scenario", "ROI area ratio", "comparison_roi_area_ratio.png"),
        ("average_fps", "Average FPS by Scenario", "FPS", "comparison_fps.png"),
        ("average_input_brightness", "Average Input Brightness by Scenario", "Brightness", "comparison_input_brightness.png"),
        ("average_output_brightness", "Average Output Brightness by Scenario", "Brightness", "comparison_output_brightness.png"),
        ("average_input_contrast", "Average Input Contrast by Scenario", "Contrast", "comparison_input_contrast.png"),
        ("average_output_contrast", "Average Output Contrast by Scenario", "Contrast", "comparison_output_contrast.png"),
        ("average_isp_ms", "Average ISP-Lite Runtime by Scenario", "ISP time (ms)", "comparison_isp_ms.png"),
    ]

    for column, title, ylabel, filename in metric_specs:
        if column not in comparison_df.columns:
            continue

        plt.figure(figsize=(8.5, 4.8))
        plt.bar(comparison_df["scenario"], pd.to_numeric(comparison_df[column], errors="coerce").fillna(0.0))
        plt.xlabel("Scenario")
        plt.ylabel(ylabel)
        plt.title(title)
        plt.tight_layout()
        plt.savefig(output_dir / filename, dpi=150)
        plt.close()


def analyze_final_runs(final_runs_dir: Path, output_dir: Path) -> None:
    scenarios = ["sparse", "night", "dense"]
    rows = []

    for scenario in scenarios:
        metrics_path = final_runs_dir / scenario / "metrics.jsonl"

        if not metrics_path.exists():
            print("")
            print("missing:", metrics_path)
            continue

        scenario_output = output_dir / scenario
        summary = analyze_one(metrics_path, scenario_output, scenario)
        row = flatten_summary(summary)
        row["scenario"] = scenario
        rows.append(row)

    if not rows:
        print("no final runs found")
        return

    comparison_df = pd.DataFrame(rows)
    first_columns = ["scenario"]
    remaining_columns = [column for column in comparison_df.columns if column not in first_columns]
    comparison_df = comparison_df[first_columns + remaining_columns]

    output_dir.mkdir(parents=True, exist_ok=True)
    comparison_df.to_csv(output_dir / "final_comparison.csv", index=False)
    plot_final_comparison(comparison_df, output_dir)

    print("")
    print("final_comparison:", output_dir / "final_comparison.csv")
    print("final_plots:", output_dir)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--metrics", type=Path, default=DEFAULT_METRICS_LOG)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT_DIR)
    parser.add_argument("--final-runs", type=Path, default=DEFAULT_FINAL_RUNS_DIR)
    parser.add_argument("--all-final", action="store_true")
    parser.add_argument("--label", type=str, default="current")
    return parser.parse_args()


def main() -> None:
    args = parse_args()

    if args.all_final:
        analyze_final_runs(args.final_runs, args.output)
        return

    analyze_one(args.metrics, args.output, args.label)
    print("")
    print("results:", args.output)


if __name__ == "__main__":
    main()