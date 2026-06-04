import json
from pathlib import Path
from typing import Any

from fastapi import FastAPI
from fastapi.responses import FileResponse, HTMLResponse
from pydantic import BaseModel

from dashboard import render_dashboard, render_empty_dashboard
from log_reader import read_last_json_line, read_recent_json_lines, summarize_metrics, summarize_webrtc


ROOT_DIR = Path(__file__).resolve().parents[1]
DEFAULT_METRICS_LOG = ROOT_DIR / "logs" / "metrics.jsonl"
DEFAULT_METADATA_LOG = ROOT_DIR / "logs" / "metadata.jsonl"
DEFAULT_WEBRTC_LOG = ROOT_DIR / "logs" / "webrtc_receiver.jsonl"
DEFAULT_OUTPUT_DIR = ROOT_DIR / "outputs"

app = FastAPI(title="VCM-Lite Edge Camera Control Plane")


@app.middleware("http")
async def no_cache_middleware(request, call_next):
    response = await call_next(request)
    response.headers["Cache-Control"] = "no-store, no-cache, must-revalidate, max-age=0"
    response.headers["Pragma"] = "no-cache"
    response.headers["Expires"] = "0"
    return response


class LogConfig(BaseModel):
    metrics_log_path: str = str(DEFAULT_METRICS_LOG)
    metadata_log_path: str = str(DEFAULT_METADATA_LOG)


state: dict[str, Any] = {
    "metrics_log_path": str(DEFAULT_METRICS_LOG),
    "metadata_log_path": str(DEFAULT_METADATA_LOG),
    "webrtc_log_path": str(DEFAULT_WEBRTC_LOG),
    "output_dir": str(DEFAULT_OUTPUT_DIR),
    "service": "vcm-lite-control-plane"
}


def read_json_file(path: Path) -> dict[str, Any]:
    if not path.exists():
        return {}

    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError:
        return {}


def read_recent_jsonl_file(path: Path, limit: int) -> list[dict[str, Any]]:
    if not path.exists():
        return []

    lines = path.read_text(encoding="utf-8", errors="ignore").splitlines()
    records: list[dict[str, Any]] = []

    for line in reversed(lines):
        stripped = line.strip()

        if not stripped:
            continue

        try:
            records.append(json.loads(stripped))
        except json.JSONDecodeError:
            continue

        if len(records) >= limit:
            break

    records.reverse()
    return records


@app.get("/health")
def health() -> dict[str, Any]:
    metrics_path = Path(state["metrics_log_path"])
    metadata_path = Path(state["metadata_log_path"])
    webrtc_path = Path(state["webrtc_log_path"])
    output_dir = Path(state["output_dir"])

    return {
        "status": "ok",
        "service": state["service"],
        "metrics_log_exists": metrics_path.exists(),
        "metadata_log_exists": metadata_path.exists(),
        "webrtc_log_exists": webrtc_path.exists(),
        "output_dir_exists": output_dir.exists(),
        "detection_event_exists": (output_dir / "latest_detection_event.json").exists(),
        "detection_history_exists": (output_dir / "detection_history.jsonl").exists(),
        "detection_frame_exists": (output_dir / "latest_detection_frame.jpg").exists(),
        "detection_crop_exists": (output_dir / "latest_detection_crop.jpg").exists(),
        "detection_reconstructed_exists": (output_dir / "latest_detection_reconstructed.jpg").exists(),
        "semantic_packet_exists": (output_dir / "latest_semantic_packet.json").exists(),
        "metrics_log_path": str(metrics_path),
        "metadata_log_path": str(metadata_path),
        "webrtc_log_path": str(webrtc_path),
        "output_dir": str(output_dir)
    }


@app.post("/config/logs")
def set_log_paths(config: LogConfig) -> dict[str, Any]:
    state["metrics_log_path"] = config.metrics_log_path
    state["metadata_log_path"] = config.metadata_log_path

    return {
        "metrics_log_path": state["metrics_log_path"],
        "metadata_log_path": state["metadata_log_path"]
    }


@app.get("/metrics/latest")
def latest_metrics() -> dict[str, Any]:
    return read_last_json_line(state["metrics_log_path"])


@app.get("/metadata/latest")
def latest_metadata() -> dict[str, Any]:
    return read_last_json_line(state["metadata_log_path"])


@app.get("/webrtc/latest")
def latest_webrtc() -> dict[str, Any]:
    return read_last_json_line(state["webrtc_log_path"])


@app.get("/webrtc/summary")
def webrtc_summary(limit: int = 200) -> dict[str, Any]:
    safe_limit = max(1, min(limit, 1000))
    records = read_recent_json_lines(state["webrtc_log_path"], safe_limit)
    return summarize_webrtc(records)


@app.get("/event/latest")
def latest_detection_event() -> dict[str, Any]:
    output_dir = Path(state["output_dir"])
    return read_json_file(output_dir / "latest_detection_event.json")


@app.get("/events/recent")
def recent_detection_events(limit: int = 20) -> list[dict[str, Any]]:
    safe_limit = max(1, min(limit, 100))
    output_dir = Path(state["output_dir"])
    return read_recent_jsonl_file(output_dir / "detection_history.jsonl", safe_limit)


@app.get("/metrics/recent")
def recent_metrics(limit: int = 50) -> list[dict[str, Any]]:
    safe_limit = max(1, min(limit, 500))
    return read_recent_json_lines(state["metrics_log_path"], safe_limit)


@app.get("/metadata/recent")
def recent_metadata(limit: int = 50) -> list[dict[str, Any]]:
    safe_limit = max(1, min(limit, 500))
    return read_recent_json_lines(state["metadata_log_path"], safe_limit)


@app.get("/summary")
def summary(limit: int = 200) -> dict[str, Any]:
    safe_limit = max(1, min(limit, 1000))
    records = read_recent_json_lines(state["metrics_log_path"], safe_limit)
    return summarize_metrics(records)


@app.get("/image/detection-frame")
def detection_frame() -> FileResponse:
    path = Path(state["output_dir"]) / "latest_detection_frame.jpg"
    return FileResponse(path)


@app.get("/image/detection-crop")
def detection_crop() -> FileResponse:
    path = Path(state["output_dir"]) / "latest_detection_crop.jpg"
    return FileResponse(path)


@app.get("/image/detection-reconstructed")
def detection_reconstructed() -> FileResponse:
    path = Path(state["output_dir"]) / "latest_detection_reconstructed.jpg"
    return FileResponse(path)


@app.get("/dashboard", response_class=HTMLResponse)
def dashboard() -> HTMLResponse:
    metrics = read_last_json_line(state["metrics_log_path"])
    metadata = read_last_json_line(state["metadata_log_path"])
    records = read_recent_json_lines(state["metrics_log_path"], 200)
    webrtc_records = read_recent_json_lines(state["webrtc_log_path"], 200)

    summary_data = summarize_metrics(records)
    webrtc_data = summarize_webrtc(webrtc_records)

    if not metrics:
        return HTMLResponse(render_empty_dashboard())

    output_dir = Path(state["output_dir"])
    event = read_json_file(output_dir / "latest_detection_event.json")
    detection_events = read_recent_jsonl_file(output_dir / "detection_history.jsonl", 20)

    image_status = {
        "detection_frame": (output_dir / "latest_detection_frame.jpg").exists(),
        "detection_crop": (output_dir / "latest_detection_crop.jpg").exists(),
        "detection_reconstructed": (output_dir / "latest_detection_reconstructed.jpg").exists()
    }

    return HTMLResponse(
        render_dashboard(
            metrics,
            metadata,
            summary_data,
            image_status,
            event,
            webrtc_data,
            detection_events
        )
    )