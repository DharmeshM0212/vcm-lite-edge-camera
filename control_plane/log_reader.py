import json
from pathlib import Path
from typing import Any


def read_last_json_line(path: str) -> dict[str, Any]:
    file_path = Path(path)

    if not file_path.exists():
        return {}

    last_line = ""

    with file_path.open("r", encoding="utf-8") as file:
        for line in file:
            stripped = line.strip()
            if stripped:
                last_line = stripped

    if not last_line:
        return {}

    try:
        return json.loads(last_line)
    except json.JSONDecodeError:
        return {}


def read_recent_json_lines(path: str, limit: int) -> list[dict[str, Any]]:
    file_path = Path(path)

    if not file_path.exists():
        return []

    lines = []

    with file_path.open("r", encoding="utf-8") as file:
        for line in file:
            stripped = line.strip()
            if stripped:
                lines.append(stripped)

    selected = lines[-limit:]

    output = []

    for line in selected:
        try:
            output.append(json.loads(line))
        except json.JSONDecodeError:
            continue

    return output


def summarize_metrics(records: list[dict[str, Any]]) -> dict[str, Any]:
    if not records:
        return {
            "frames": 0
        }

    def avg(key: str) -> float:
        values = [float(item.get(key, 0.0)) for item in records]
        return sum(values) / max(1, len(values))

    latest = records[-1]

    return {
        "frames": len(records),
        "latest_frame_id": latest.get("frame_id", 0),
        "average_fps": avg("fps"),
        "average_latency_ms": avg("latency_ms"),
        "average_bitrate_kbps": avg("bitrate_kbps"),
        "average_ai_stability_loss": avg("ai_stability_loss"),
        "average_roi_count": avg("roi_count"),
        "latest_controller_state": latest.get("controller_state", ""),
        "latest_mode": latest.get("mode", ""),
        "latest_context_width": latest.get("context_width", 0),
        "latest_roi_tile_width": latest.get("roi_tile_width", 0),
        "latest_roi_quality": latest.get("roi_quality", 0),
        "latest_context_quality": latest.get("context_quality", 0),
        "latest_dropped_frames": latest.get("dropped_frames", 0),
        "latest_queue_depth": latest.get("queue_depth", 0)
    }


def summarize_webrtc(records: list[dict[str, Any]]) -> dict[str, Any]:
    if not records:
        return {
            "active": False,
            "frames": 0
        }

    latest = records[-1]

    return {
        "active": True,
        "frames": len(records),
        "latest_frame_id": latest.get("frame_id", 0),
        "fps": latest.get("fps", 0.0),
        "width": latest.get("width", 0),
        "height": latest.get("height", 0),
        "socket_clients": latest.get("socket_clients", 0),
        "timestamp": latest.get("timestamp", 0.0)
    }