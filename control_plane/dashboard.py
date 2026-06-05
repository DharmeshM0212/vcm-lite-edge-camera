from html import escape
from time import time
from typing import Any


def fmt(value: Any, digits: int = 2) -> str:
    if value is None:
        return "-"

    if isinstance(value, float):
        return f"{value:.{digits}f}"

    return str(value)


def status_class(metrics: dict[str, Any]) -> str:
    state = str(metrics.get("controller_state", "")).lower()

    if "ai_repair" in state:
        return "warn"

    if "overload" in state:
        return "bad"

    if "rate" in state or "protect" in state or "watch" in state:
        return "warn"

    return "ok"


def image_card(title: str, endpoint: str, exists: bool) -> str:
    version = str(int(time() * 1000))

    if not exists:
        return f"""
        <div class="image-card">
            <div class="image-title">{escape(title)}</div>
            <div class="image-placeholder">Image not available</div>
        </div>
        """

    return f"""
    <div class="image-card">
        <div class="image-title">{escape(title)}</div>
        <img src="{endpoint}?t={version}" alt="{escape(title)}">
    </div>
    """


def primary_object(event: dict[str, Any]) -> dict[str, Any]:
    obj = event.get("primary_object", {})

    if isinstance(obj, dict):
        return obj

    return {}


def render_event_panel(event: dict[str, Any]) -> str:
    if not event:
        return """
        <div class="panel">
            <h2>Last Object Detection Event</h2>
            <p class="muted">No confirmed object event yet. The validation snapshots can still update even without a confirmed object.</p>
        </div>
        """

    primary = primary_object(event)
    label = escape(str(primary.get("label", "-")))

    return f"""
    <div class="panel">
        <h2>Last Object Detection Event</h2>
        <div class="kv">
            <div class="key">Frame ID</div>
            <div class="value">{fmt(event.get("frame_id"), 0)}</div>
            <div class="key">Primary object</div>
            <div class="value">{label}</div>
            <div class="key">Object confidence</div>
            <div class="value">{fmt(primary.get("confidence"), 3)}</div>
            <div class="key">Bounding box</div>
            <div class="value">x={fmt(primary.get("x"), 0)}, y={fmt(primary.get("y"), 0)}, w={fmt(primary.get("width"), 0)}, h={fmt(primary.get("height"), 0)}</div>
            <div class="key">Objects in event</div>
            <div class="value">{fmt(event.get("object_count"), 0)}</div>
            <div class="key">Reference confidence</div>
            <div class="value">{fmt(event.get("reference_ai_confidence"), 3)}</div>
            <div class="key">Compressed confidence</div>
            <div class="value">{fmt(event.get("compressed_ai_confidence"), 3)}</div>
            <div class="key">Detector confidence loss</div>
            <div class="value">{fmt(event.get("detector_confidence_loss"), 3)}</div>
            <div class="key">AI stability loss</div>
            <div class="value">{fmt(event.get("ai_stability_loss"), 3)}</div>
        </div>
    </div>
    """


def render_validation_panel(validation: dict[str, Any]) -> str:
    if not validation:
        return """
        <div class="panel">
            <h2>Last Semantic Validation</h2>
            <p class="muted">No semantic validation snapshot has been written yet.</p>
        </div>
        """

    return f"""
    <div class="panel">
        <h2>Last Semantic Validation</h2>
        <div class="kv">
            <div class="key">Frame ID</div>
            <div class="value">{fmt(validation.get("frame_id"), 0)}</div>
            <div class="key">Type</div>
            <div class="value">{escape(str(validation.get("validation_type", "-")))}</div>
            <div class="key">Reference confidence</div>
            <div class="value">{fmt(validation.get("reference_ai_confidence"), 3)}</div>
            <div class="key">Compressed confidence</div>
            <div class="value">{fmt(validation.get("compressed_ai_confidence"), 3)}</div>
            <div class="key">Detector confidence loss</div>
            <div class="value">{fmt(validation.get("detector_confidence_loss"), 3)}</div>
            <div class="key">AI stability loss</div>
            <div class="value">{fmt(validation.get("ai_stability_loss"), 3)}</div>
            <div class="key">Detector ran</div>
            <div class="value">{escape(str(validation.get("detector_ran", "-")))}</div>
            <div class="key">Compressed validation ran</div>
            <div class="value">{escape(str(validation.get("compressed_validation_ran", "-")))}</div>
        </div>
    </div>
    """


def render_recent_events_panel(events: list[dict[str, Any]]) -> str:
    if not events:
        return """
        <div class="panel">
            <h2>Recent Object Events</h2>
            <p class="muted">No confirmed object events yet.</p>
        </div>
        """

    rows = []

    for event in reversed(events[-12:]):
        obj = primary_object(event)
        rows.append(
            f"""
            <tr>
                <td>{fmt(event.get("frame_id"), 0)}</td>
                <td>{escape(str(obj.get("label", "-")))}</td>
                <td>{fmt(obj.get("confidence"), 3)}</td>
                <td>{fmt(event.get("object_count"), 0)}</td>
                <td>{fmt(event.get("ai_stability_loss"), 3)}</td>
                <td>{fmt(event.get("detector_confidence_loss"), 3)}</td>
            </tr>
            """
        )

    return f"""
    <div class="panel">
        <h2>Recent Object Events</h2>
        <table class="event-table">
            <thead>
                <tr>
                    <th>Frame</th>
                    <th>Object</th>
                    <th>Conf.</th>
                    <th>Count</th>
                    <th>AI Loss</th>
                    <th>Conf. Loss</th>
                </tr>
            </thead>
            <tbody>{''.join(rows)}</tbody>
        </table>
    </div>
    """


def render_recent_validations_panel(validations: list[dict[str, Any]]) -> str:
    if not validations:
        return """
        <div class="panel">
            <h2>Recent Semantic Validations</h2>
            <p class="muted">No validation history yet.</p>
        </div>
        """

    rows = []

    for event in reversed(validations[-10:]):
        rows.append(
            f"""
            <tr>
                <td>{fmt(event.get("frame_id"), 0)}</td>
                <td>{escape(str(event.get("validation_type", "-")))}</td>
                <td>{fmt(event.get("reference_ai_confidence"), 3)}</td>
                <td>{fmt(event.get("compressed_ai_confidence"), 3)}</td>
                <td>{fmt(event.get("ai_stability_loss"), 3)}</td>
            </tr>
            """
        )

    return f"""
    <div class="panel">
        <h2>Recent Semantic Validations</h2>
        <table class="event-table">
            <thead>
                <tr>
                    <th>Frame</th>
                    <th>Type</th>
                    <th>Ref.</th>
                    <th>Comp.</th>
                    <th>AI Loss</th>
                </tr>
            </thead>
            <tbody>{''.join(rows)}</tbody>
        </table>
    </div>
    """


def render_webrtc_panel(webrtc: dict[str, Any]) -> str:
    status = "active" if webrtc.get("active", False) else "inactive"

    return f"""
    <div class="panel">
        <h2>WebRTC Ingest</h2>
        <div class="kv">
            <div class="key">Status</div>
            <div class="value">{status}</div>
            <div class="key">Receiver frame ID</div>
            <div class="value">{fmt(webrtc.get("latest_frame_id"), 0)}</div>
            <div class="key">Receiver FPS</div>
            <div class="value">{fmt(webrtc.get("fps"), 2)}</div>
            <div class="key">Input frame size</div>
            <div class="value">{fmt(webrtc.get("width"), 0)} × {fmt(webrtc.get("height"), 0)}</div>
            <div class="key">Socket clients</div>
            <div class="value">{fmt(webrtc.get("socket_clients"), 0)}</div>
        </div>
    </div>
    """


def render_controller_panel(metrics: dict[str, Any], summary: dict[str, Any]) -> str:
    return f"""
    <div class="panel">
        <h2>Adaptive Controller</h2>
        <div class="kv">
            <div class="key">State</div>
            <div class="value">{escape(str(metrics.get("controller_state", "-")))}</div>
            <div class="key">Mode</div>
            <div class="value">{escape(str(metrics.get("controller_mode", "-")))}</div>
            <div class="key">Reason</div>
            <div class="value">{escape(str(metrics.get("controller_reason", "-")))}</div>
            <div class="key">Action</div>
            <div class="value">{escape(str(metrics.get("controller_action", "-")))}</div>
            <div class="key">Detector interval</div>
            <div class="value">{fmt(metrics.get("detector_interval"), 0)}</div>
            <div class="key">ROI quality</div>
            <div class="value">{fmt(metrics.get("roi_quality"), 0)}</div>
            <div class="key">Context quality</div>
            <div class="value">{fmt(metrics.get("context_quality"), 0)}</div>
            <div class="key">Context size</div>
            <div class="value">{fmt(metrics.get("context_width"), 0)} × {fmt(metrics.get("context_height"), 0)}</div>
            <div class="key">Re-encode allowed</div>
            <div class="value">{escape(str(metrics.get("reencode_allowed", "-")))}</div>
            <div class="key">Re-encoded</div>
            <div class="value">{escape(str(metrics.get("reencoded", "-")))}</div>
            <div class="key">Dropped frames</div>
            <div class="value">{fmt(metrics.get("dropped_frames"), 0)}</div>
            <div class="key">Queue depth</div>
            <div class="value">{fmt(metrics.get("queue_depth"), 0)}</div>
            <div class="key">Avg bitrate</div>
            <div class="value">{fmt(summary.get("average_bitrate_kbps"), 1)} kbps</div>
        </div>
    </div>
    """


def render_ai_panel(metrics: dict[str, Any], event: dict[str, Any], validation: dict[str, Any]) -> str:
    primary = primary_object(event)
    last_event_frame = event.get("frame_id", "-") if event else "-"
    last_object = primary.get("label", "-") if event else "-"
    last_conf = primary.get("confidence", None) if event else None
    validation_frame = validation.get("frame_id", "-") if validation else "-"

    detector_status = "ran this frame" if bool(metrics.get("detector_ran", False)) else "scheduled / reused"

    return f"""
    <div class="panel">
        <h2>AI / Detector</h2>
        <div class="kv">
            <div class="key">Detector status</div>
            <div class="value">{detector_status}</div>
            <div class="key">Last validation frame</div>
            <div class="value">{fmt(validation_frame, 0)}</div>
            <div class="key">Last event frame</div>
            <div class="value">{fmt(last_event_frame, 0)}</div>
            <div class="key">Last object</div>
            <div class="value">{escape(str(last_object))}</div>
            <div class="key">Last object confidence</div>
            <div class="value">{fmt(last_conf, 3)}</div>
            <div class="key">Current detected objects</div>
            <div class="value">{fmt(metrics.get("detected_object_count"), 0)}</div>
            <div class="key">Detector ran</div>
            <div class="value">{escape(str(metrics.get("detector_ran", "-")))}</div>
            <div class="key">DNN runtime</div>
            <div class="value">{escape(str(metrics.get("detector_used_dnn", "-")))}</div>
            <div class="key">Max raw confidence</div>
            <div class="value">{fmt(metrics.get("max_detector_confidence"), 3)}</div>
            <div class="key">Last reference confidence</div>
            <div class="value">{fmt(validation.get("reference_ai_confidence") if validation else metrics.get("reference_ai_confidence"), 3)}</div>
            <div class="key">Last compressed confidence</div>
            <div class="value">{fmt(validation.get("compressed_ai_confidence") if validation else metrics.get("compressed_ai_confidence"), 3)}</div>
            <div class="key">Last confidence loss</div>
            <div class="value">{fmt(validation.get("detector_confidence_loss") if validation else metrics.get("detector_confidence_loss"), 3)}</div>
            <div class="key">AI stability loss</div>
            <div class="value">{fmt(validation.get("ai_stability_loss") if validation else metrics.get("ai_stability_loss"), 3)}</div>
            <div class="key">Mode</div>
            <div class="value">{escape(str(metrics.get("mode", "-")))}</div>
        </div>
    </div>
    """


def render_dashboard(
    metrics: dict[str, Any],
    metadata: dict[str, Any],
    summary: dict[str, Any],
    image_status: dict[str, bool],
    event: dict[str, Any],
    validation: dict[str, Any],
    webrtc: dict[str, Any],
    detection_events: list[dict[str, Any]],
    validation_events: list[dict[str, Any]]
) -> str:
    state_class = status_class(metrics)
    controller_state = escape(str(metrics.get("controller_state", "-")))
    mode = escape(str(metrics.get("mode", "-")))
    isp_profile = escape(str(metrics.get("isp_profile", "-")))

    event_panel = render_event_panel(event)
    validation_panel = render_validation_panel(validation)
    recent_events_panel = render_recent_events_panel(detection_events)
    recent_validations_panel = render_recent_validations_panel(validation_events)
    webrtc_panel = render_webrtc_panel(webrtc)
    controller_panel = render_controller_panel(metrics, summary)
    ai_panel = render_ai_panel(metrics, event, validation)

    frame_endpoint = "/image/detection-frame" if image_status.get("detection_frame", False) else "/image/validation-frame"
    frame_exists = image_status.get("detection_frame", False) or image_status.get("validation_frame", False)
    frame_title = "Latest Object Frame" if image_status.get("detection_frame", False) else "Latest Validation Frame"

    recon_endpoint = "/image/detection-reconstructed" if image_status.get("detection_reconstructed", False) else "/image/validation-reconstructed"
    recon_exists = image_status.get("detection_reconstructed", False) or image_status.get("validation_reconstructed", False)
    recon_title = "Semantic Reconstruction"

    detection_frame_card = image_card(frame_title, frame_endpoint, frame_exists)
    detection_crop_card = image_card("Detected Object Crop", "/image/detection-crop", image_status.get("detection_crop", False))
    detection_reconstructed_card = image_card(recon_title, recon_endpoint, recon_exists)

    page_version = str(int(time() * 1000))

    return f"""
<!doctype html>
<html>
<head>
    <meta charset="utf-8">
    <title>VCM-Lite Edge Camera</title>
    <meta http-equiv="Cache-Control" content="no-store, no-cache, must-revalidate, max-age=0">
    <meta http-equiv="Pragma" content="no-cache">
    <meta http-equiv="Expires" content="0">
    <script>
        setTimeout(function () {{
            window.location.replace("/dashboard?t={page_version}&r=" + Date.now());
        }}, 2000);
    </script>
    <style>
        :root {{
            --bg: #f6f7f9;
            --panel: #ffffff;
            --text: #1f2937;
            --muted: #6b7280;
            --line: #e5e7eb;
            --ok: #166534;
            --ok-bg: #dcfce7;
            --warn: #92400e;
            --warn-bg: #fef3c7;
            --bad: #991b1b;
            --bad-bg: #fee2e2;
        }}
        body {{
            margin: 0;
            font-family: Arial, Helvetica, sans-serif;
            background: var(--bg);
            color: var(--text);
        }}
        header {{
            background: #111827;
            color: #ffffff;
            padding: 18px 28px;
            border-bottom: 1px solid #0f172a;
        }}
        header h1 {{
            margin: 0;
            font-size: 20px;
            font-weight: 600;
        }}
        header p {{
            margin: 6px 0 0 0;
            color: #cbd5e1;
            font-size: 13px;
        }}
        main {{
            padding: 24px 28px;
            max-width: 1400px;
            margin: 0 auto;
        }}
        .topbar {{
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 18px;
            gap: 12px;
        }}
        .badge {{
            padding: 6px 10px;
            border-radius: 999px;
            font-size: 13px;
            font-weight: 600;
            display: inline-block;
        }}
        .ok {{
            background: var(--ok-bg);
            color: var(--ok);
        }}
        .warn {{
            background: var(--warn-bg);
            color: var(--warn);
        }}
        .bad {{
            background: var(--bad-bg);
            color: var(--bad);
        }}
        .grid {{
            display: grid;
            grid-template-columns: repeat(4, 1fr);
            gap: 14px;
            margin-bottom: 18px;
        }}
        .panel {{
            background: var(--panel);
            border: 1px solid var(--line);
            border-radius: 10px;
            padding: 16px;
            box-shadow: 0 1px 2px rgba(0,0,0,0.04);
            margin-bottom: 18px;
        }}
        .panel h2 {{
            margin: 0 0 12px 0;
            font-size: 15px;
            font-weight: 600;
            color: #111827;
        }}
        .metric-label {{
            color: var(--muted);
            font-size: 12px;
            margin-bottom: 6px;
        }}
        .metric-value {{
            font-size: 26px;
            font-weight: 700;
            color: #111827;
        }}
        .metric-unit {{
            font-size: 13px;
            color: var(--muted);
            margin-left: 4px;
            font-weight: 400;
        }}
        .section-grid {{
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 14px;
            margin-bottom: 18px;
        }}
        .image-grid {{
            display: grid;
            grid-template-columns: 1.4fr 0.8fr 1.4fr;
            gap: 14px;
            margin-bottom: 18px;
        }}
        .image-card {{
            background: var(--panel);
            border: 1px solid var(--line);
            border-radius: 10px;
            padding: 12px;
            box-shadow: 0 1px 2px rgba(0,0,0,0.04);
        }}
        .image-title {{
            font-size: 13px;
            font-weight: 600;
            color: #111827;
            margin-bottom: 10px;
        }}
        .image-card img {{
            width: 100%;
            height: 260px;
            object-fit: contain;
            background: #0b1220;
            border-radius: 6px;
            display: block;
        }}
        .image-placeholder {{
            height: 260px;
            border-radius: 6px;
            background: #eef2f7;
            display: flex;
            align-items: center;
            justify-content: center;
            color: var(--muted);
            font-size: 13px;
        }}
        .kv {{
            display: grid;
            grid-template-columns: 190px 1fr;
            row-gap: 8px;
            column-gap: 12px;
            font-size: 13px;
        }}
        .key {{
            color: var(--muted);
        }}
        .value {{
            font-weight: 600;
        }}
        .muted {{
            color: var(--muted);
        }}
        .event-table {{
            width: 100%;
            border-collapse: collapse;
            font-size: 13px;
        }}
        .event-table th {{
            text-align: left;
            color: var(--muted);
            font-weight: 600;
            border-bottom: 1px solid var(--line);
            padding: 8px 6px;
        }}
        .event-table td {{
            border-bottom: 1px solid var(--line);
            padding: 8px 6px;
            font-weight: 600;
        }}
        footer {{
            color: var(--muted);
            font-size: 12px;
            padding: 10px 0 24px 0;
        }}
        @media (max-width: 1000px) {{
            .grid {{
                grid-template-columns: repeat(2, 1fr);
            }}
            .section-grid {{
                grid-template-columns: 1fr;
            }}
            .image-grid {{
                grid-template-columns: 1fr;
            }}
        }}
        @media (max-width: 640px) {{
            .grid {{
                grid-template-columns: 1fr;
            }}
            main {{
                padding: 18px;
            }}
        }}
    </style>
</head>
<body>
    <header>
        <h1>VCM-Lite Edge Camera</h1>
        <p>Object-aware semantic video pipeline observability</p>
    </header>
    <main>
        <div class="topbar">
            <div>
                <span class="badge {state_class}">{controller_state}</span>
                <span class="badge ok">{mode}</span>
            </div>
            <div class="muted">Auto-refresh: 2s</div>
        </div>

        <div class="grid">
            <div class="panel">
                <div class="metric-label">FPS</div>
                <div class="metric-value">{fmt(metrics.get("fps"), 2)}</div>
            </div>
            <div class="panel">
                <div class="metric-label">Latency</div>
                <div class="metric-value">{fmt(metrics.get("latency_ms"), 1)}<span class="metric-unit">ms</span></div>
            </div>
            <div class="panel">
                <div class="metric-label">Bitrate</div>
                <div class="metric-value">{fmt(metrics.get("bitrate_kbps"), 1)}<span class="metric-unit">kbps</span></div>
            </div>
            <div class="panel">
                <div class="metric-label">AI Stability Loss</div>
                <div class="metric-value">{fmt(validation.get("ai_stability_loss") if validation else metrics.get("ai_stability_loss"), 3)}</div>
            </div>
        </div>

        {webrtc_panel}

        <div class="image-grid">
            {detection_frame_card}
            {detection_crop_card}
            {detection_reconstructed_card}
        </div>

        <div class="section-grid">
            {event_panel}
            {validation_panel}
        </div>

        <div class="section-grid">
            {recent_events_panel}
            {recent_validations_panel}
        </div>

        <div class="section-grid">
            <div class="panel">
                <h2>Semantic Packet</h2>
                <div class="kv">
                    <div class="key">Context size</div>
                    <div class="value">{fmt(metrics.get("context_width"), 0)} × {fmt(metrics.get("context_height"), 0)}</div>
                    <div class="key">ROI tile size</div>
                    <div class="value">{fmt(metrics.get("roi_tile_width"), 0)} × {fmt(metrics.get("roi_tile_height"), 0)}</div>
                    <div class="key">ROI count</div>
                    <div class="value">{fmt(metrics.get("roi_count"), 0)}</div>
                    <div class="key">ROI area ratio</div>
                    <div class="value">{fmt(metrics.get("roi_area_ratio"), 3)}</div>
                    <div class="key">ROI quality</div>
                    <div class="value">{fmt(metrics.get("roi_quality"), 0)}</div>
                    <div class="key">Context quality</div>
                    <div class="value">{fmt(metrics.get("context_quality"), 0)}</div>
                    <div class="key">Total encoded bytes</div>
                    <div class="value">{fmt(metrics.get("total_encoded_bytes"), 0)}</div>
                </div>
            </div>
            {controller_panel}
        </div>

        <div class="section-grid">
            <div class="panel">
                <h2>ISP-Lite</h2>
                <div class="kv">
                    <div class="key">Profile</div>
                    <div class="value">{isp_profile}</div>
                    <div class="key">Input brightness</div>
                    <div class="value">{fmt(metrics.get("input_brightness"), 2)}</div>
                    <div class="key">Output brightness</div>
                    <div class="value">{fmt(metrics.get("mean_brightness"), 2)}</div>
                    <div class="key">Input contrast</div>
                    <div class="value">{fmt(metrics.get("input_contrast"), 2)}</div>
                    <div class="key">Output contrast</div>
                    <div class="value">{fmt(metrics.get("output_contrast"), 2)}</div>
                    <div class="key">Brightness gain</div>
                    <div class="value">{fmt(metrics.get("brightness_gain"), 2)}</div>
                    <div class="key">Gamma</div>
                    <div class="value">{fmt(metrics.get("gamma"), 2)}</div>
                    <div class="key">Denoise strength</div>
                    <div class="value">{fmt(metrics.get("denoise_strength"), 2)}</div>
                    <div class="key">CLAHE enabled</div>
                    <div class="value">{escape(str(metrics.get("clahe_enabled", "-")))}</div>
                </div>
            </div>
            {ai_panel}
        </div>

        <footer>
            VCM-Lite Edge Camera observability plane. Validation snapshots and object events are generated by the C++ engine.
        </footer>
    </main>
</body>
</html>
"""


def render_empty_dashboard() -> str:
    return """
<!doctype html>
<html>
<head>
    <meta charset="utf-8">
    <title>VCM-Lite Edge Camera</title>
    <meta http-equiv="Cache-Control" content="no-store, no-cache, must-revalidate, max-age=0">
    <meta http-equiv="Pragma" content="no-cache">
    <meta http-equiv="Expires" content="0">
    <script>
        setTimeout(function () {
            window.location.replace("/dashboard?t=" + Date.now());
        }, 2000);
    </script>
    <style>
        body {
            margin: 0;
            font-family: Arial, Helvetica, sans-serif;
            background: #f6f7f9;
            color: #1f2937;
        }
        header {
            background: #111827;
            color: white;
            padding: 18px 28px;
        }
        main {
            padding: 24px 28px;
        }
        .panel {
            background: white;
            border: 1px solid #e5e7eb;
            border-radius: 10px;
            padding: 18px;
            max-width: 720px;
        }
        .muted {
            color: #6b7280;
        }
    </style>
</head>
<body>
    <header>
        <h1>VCM-Lite Edge Camera</h1>
    </header>
    <main>
        <div class="panel">
            <h2>No metrics available</h2>
            <p class="muted">Run the C++ engine first so it writes logs/metrics.jsonl and output snapshots.</p>
        </div>
    </main>
</body>
</html>
"""