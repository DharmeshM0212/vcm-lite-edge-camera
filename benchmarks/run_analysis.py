from pathlib import Path

from analyze_metrics import load_metrics_jsonl, summarize_metrics, write_summary
from plot_metrics import plot_all


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_METRICS_LOG = ROOT / "logs" / "metrics.jsonl"
DEFAULT_OUTPUT_DIR = ROOT / "benchmarks" / "results"


def main() -> None:
    df = load_metrics_jsonl(DEFAULT_METRICS_LOG)
    summary = summarize_metrics(df)

    DEFAULT_OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

    write_summary(summary, DEFAULT_OUTPUT_DIR)
    plot_all(df, DEFAULT_OUTPUT_DIR)

    print("frames:", summary.get("frames", 0))
    print("average_fps:", round(summary.get("average_fps", 0.0), 3))
    print("average_latency_ms:", round(summary.get("average_latency_ms", 0.0), 3))
    print("p95_latency_ms:", round(summary.get("p95_latency_ms", 0.0), 3))
    print("average_bitrate_kbps:", round(summary.get("average_bitrate_kbps", 0.0), 3))
    print("average_ai_stability_loss:", round(summary.get("average_ai_stability_loss", 0.0), 4))
    print("results:", DEFAULT_OUTPUT_DIR)


if __name__ == "__main__":
    main()