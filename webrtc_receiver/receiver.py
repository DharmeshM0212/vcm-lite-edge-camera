import argparse
import asyncio
import json
import struct
import time
from pathlib import Path
from typing import Any

import cv2
from aiortc import RTCPeerConnection, RTCSessionDescription
from av import VideoFrame

from http_signaling import HttpSignaling


RAW_FRAME_MAGIC = b"VCMR"


async def wait_for_ice_gathering_complete(pc: RTCPeerConnection) -> None:
    if pc.iceGatheringState == "complete":
        return

    event = asyncio.Event()

    @pc.on("icegatheringstatechange")
    def on_icegatheringstatechange() -> None:
        if pc.iceGatheringState == "complete":
            event.set()

    await event.wait()


class FrameSocketServer:
    def __init__(self) -> None:
        self.clients: set[asyncio.StreamWriter] = set()
        self.server: asyncio.base_events.Server | None = None

    async def start(self, host: str, port: int) -> None:
        self.server = await asyncio.start_server(self.handle_client, host, port)
        print("frame_socket_listening:", f"{host}:{port}")

    async def handle_client(self, reader: asyncio.StreamReader, writer: asyncio.StreamWriter) -> None:
        peer = writer.get_extra_info("peername")
        print("frame_socket_client_connected:", peer)
        self.clients.add(writer)

        try:
            while not reader.at_eof():
                await asyncio.sleep(1.0)
        finally:
            self.clients.discard(writer)
            writer.close()
            try:
                await writer.wait_closed()
            except Exception:
                pass
            print("frame_socket_client_disconnected:", peer)

    async def broadcast_frame(self, image) -> None:
        if not self.clients:
            return

        if image is None or image.size == 0:
            return

        if not image.flags["C_CONTIGUOUS"]:
            image = image.copy()

        height, width = image.shape[:2]

        if len(image.shape) == 2:
            image = cv2.cvtColor(image, cv2.COLOR_GRAY2BGR)
            height, width = image.shape[:2]

        channels = image.shape[2]

        if channels != 3:
            image = image[:, :, :3].copy()
            channels = 3

        payload = image.tobytes()
        payload_size = len(payload)

        header = RAW_FRAME_MAGIC + struct.pack("!IIII", int(width), int(height), int(channels), int(payload_size))
        packet = header + payload

        dead_clients = []

        for writer in list(self.clients):
            try:
                writer.write(packet)
                await writer.drain()
            except Exception:
                dead_clients.append(writer)

        for writer in dead_clients:
            self.clients.discard(writer)
            writer.close()
            try:
                await writer.wait_closed()
            except Exception:
                pass


def write_jsonl(path: Path, data: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)

    with path.open("a", encoding="utf-8") as file:
        file.write(json.dumps(data) + "\n")


async def consume_video(
    track,
    output_dir: Path,
    log_path: Path,
    frame_server: FrameSocketServer,
    save_debug_frames: bool
) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)

    frame_count = 0
    window_start = time.time()
    window_frames = 0
    fps = 0.0

    while True:
        frame: VideoFrame = await track.recv()
        now = time.time()

        image = frame.to_ndarray(format="bgr24")
        frame_count += 1
        window_frames += 1

        elapsed = now - window_start

        if elapsed >= 1.0:
            fps = window_frames / elapsed
            window_frames = 0
            window_start = now

        await frame_server.broadcast_frame(image)

        if save_debug_frames and frame_count % 10 == 0:
            cv2.imwrite(str(output_dir / "latest_webrtc_frame.jpg"), image)

        write_jsonl(
            log_path,
            {
                "frame_id": frame_count,
                "timestamp": now,
                "fps": fps,
                "width": int(image.shape[1]),
                "height": int(image.shape[0]),
                "socket_clients": len(frame_server.clients),
                "bridge_format": "raw_bgr",
                "raw_frame_bytes": int(image.shape[0] * image.shape[1] * image.shape[2]),
            },
        )

        if frame_count % 30 == 0:
            print(
                "received_frames:",
                frame_count,
                "fps:",
                round(fps, 2),
                "socket_clients:",
                len(frame_server.clients),
                "bridge:",
                "raw_bgr",
            )


async def run(args: argparse.Namespace) -> None:
    signaling = HttpSignaling(args.signaling_url)
    frame_server = FrameSocketServer()
    await frame_server.start(args.frame_host, args.frame_port)

    pc = RTCPeerConnection()

    @pc.on("connectionstatechange")
    async def on_connectionstatechange() -> None:
        print("connection_state:", pc.connectionState)

    @pc.on("iceconnectionstatechange")
    async def on_iceconnectionstatechange() -> None:
        print("ice_connection_state:", pc.iceConnectionState)

    @pc.on("track")
    def on_track(track) -> None:
        print("track_received:", track.kind)

        if track.kind == "video":
            asyncio.create_task(
                consume_video(
                    track,
                    Path(args.output_dir),
                    Path(args.log_path),
                    frame_server,
                    args.save_debug_frames,
                )
            )

    print("waiting_for_offer:", args.signaling_url)
    offer_data = await signaling.wait_for_offer()
    offer = RTCSessionDescription(sdp=offer_data["sdp"], type=offer_data["type"])

    await pc.setRemoteDescription(offer)

    answer = await pc.createAnswer()
    await pc.setLocalDescription(answer)
    await wait_for_ice_gathering_complete(pc)

    await signaling.write_answer(
        {
            "sdp": pc.localDescription.sdp,
            "type": pc.localDescription.type,
        }
    )

    print("answer_posted:", args.signaling_url)

    try:
        while True:
            await asyncio.sleep(1.0)
    except KeyboardInterrupt:
        pass
    finally:
        await pc.close()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--signaling-url", default="http://127.0.0.1:9000")
    parser.add_argument("--output-dir", default="../outputs")
    parser.add_argument("--log-path", default="../logs/webrtc_receiver.jsonl")
    parser.add_argument("--frame-host", default="127.0.0.1")
    parser.add_argument("--frame-port", type=int, default=5001)
    parser.add_argument("--save-debug-frames", action="store_true")
    return parser.parse_args()


def main() -> None:
    asyncio.run(run(parse_args()))


if __name__ == "__main__":
    main()