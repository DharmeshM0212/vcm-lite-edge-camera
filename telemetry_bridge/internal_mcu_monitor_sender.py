import argparse
import json
import socket
import time
from pathlib import Path
from typing import Any


def read_last_json_line(path: Path) -> dict[str, Any]:
    if not path.exists():
        return {}

    last = ""

    with path.open("r", encoding="utf-8") as file:
        for line in file:
            stripped = line.strip()
            if stripped:
                last = stripped

    if not last:
        return {}

    try:
        return json.loads(last)
    except json.JSONDecodeError:
        return {}


def classify_health(metrics: dict[str, Any]) -> str:
    fps = float(metrics.get("fps", 0.0))
    latency = float(metrics.get("latency_ms", 0.0))
    ai_loss = float(metrics.get("ai_stability_loss", 0.0))
    queue_depth = int(metrics.get("queue_depth", 0))
    state = str(metrics.get("controller_state", "")).lower()

    if ai_loss > 0.35 or latency > 650.0:
        return "RED"

    if queue_depth >= 3 and latency > 450.0:
        return "RED"

    if ai_loss > 0.15:
        return "YELLOW"

    if latency > 280.0:
        return "YELLOW"

    if fps > 0.1 and fps < 4.0:
        return "YELLOW"

    if "overload" in state:
        return "YELLOW"

    return "GREEN"


def make_packet(metrics: dict[str, Any]) -> dict[str, Any]:
    return {
        "type": "vcm_telemetry",
        "frame_id": int(metrics.get("frame_id", 0)),
        "fps": round(float(metrics.get("fps", 0.0)), 2),
        "latency_ms": round(float(metrics.get("latency_ms", 0.0)), 2),
        "bitrate_kbps": round(float(metrics.get("bitrate_kbps", 0.0)), 2),
        "ai_loss": round(float(metrics.get("ai_stability_loss", 0.0)), 3),
        "detector_loss": round(float(metrics.get("detector_confidence_loss", 0.0)), 3),
        "queue_depth": int(metrics.get("queue_depth", 0)),
        "dropped_frames": int(metrics.get("dropped_frames", 0)),
        "cpu_percent": round(float(metrics.get("cpu_percent", 0.0)), 2),
        "ram_mb": round(float(metrics.get("ram_mb", 0.0)), 2),
        "controller_state": str(metrics.get("controller_state", "")),
        "controller_mode": str(metrics.get("controller_mode", "")),
        "health": classify_health(metrics),
    }


def connect_monitor(host: str, port: int) -> socket.socket:
    while True:
        try:
            sock = socket.create_connection((host, port), timeout=5.0)
            sock.settimeout(0.2)
            print(f"connected_to_monitor:{host}:{port}")
            return sock
        except OSError as error:
            print(f"monitor_connect_wait:{host}:{port}:{error}")
            time.sleep(1.0)


def drain_monitor(sock: socket.socket) -> None:
    try:
        data = sock.recv(4096)
        if data:
            text = data.decode("utf-8", errors="ignore").strip()
            if text:
                print("mcu:", text)
    except socket.timeout:
        pass
    except OSError:
        raise


def run(args: argparse.Namespace) -> None:
    metrics_path = Path(args.metrics_log)
    sock = connect_monitor(args.host, args.port)
    last_frame_id = None

    try:
        while True:
            metrics = read_last_json_line(metrics_path)

            if metrics:
                frame_id = int(metrics.get("frame_id", -1))

                if args.send_repeated or frame_id != last_frame_id:
                    packet = make_packet(metrics)
                    line = json.dumps(packet, separators=(",", ":")) + "\n"

                    try:
                        sock.sendall(line.encode("utf-8"))
                        print(line.strip())
                        last_frame_id = frame_id
                    except OSError:
                        try:
                            sock.close()
                        except OSError:
                            pass

                        sock = connect_monitor(args.host, args.port)

            try:
                drain_monitor(sock)
            except OSError:
                try:
                    sock.close()
                except OSError:
                    pass

                sock = connect_monitor(args.host, args.port)

            time.sleep(args.interval)
    except KeyboardInterrupt:
        pass
    finally:
        sock.close()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--metrics-log", default="../logs/metrics.jsonl")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=7500)
    parser.add_argument("--interval", type=float, default=0.25)
    parser.add_argument("--send-repeated", action="store_true")
    return parser.parse_args()


def main() -> None:
    run(parse_args())


if __name__ == "__main__":
    main()