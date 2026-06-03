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

from signaling import FileSignaling


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

    async def broadcast_jpeg(self, image) -> None:
        if not self.clients:
            return

        ok, encoded = cv2.imencode(".jpg", image, [int(cv2.IMWRITE_JPEG_QUALITY), 90])

        if not ok:
            return

        payload = encoded.tobytes()
        header = struct.pack("!I", len(payload))
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


async def wait_for_offer(signaling: FileSignaling) -> dict:
    while True:
        offer = signaling.read("offer")

        if offer is not None:
            return offer

        await asyncio.sleep(0.25)


async def consume_video(track, output_dir: Path, log_path: Path, frame_server: FrameSocketServer, save_debug_frames: bool) -> None:
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

        await frame_server.broadcast_jpeg(image)

        if save_debug_frames and frame_count % 5 == 0:
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
            },
        )

        if frame_count % 30 == 0:
            print("received_frames:", frame_count, "fps:", round(fps, 2), "socket_clients:", len(frame_server.clients))


async def run(args: argparse.Namespace) -> None:
    signaling = FileSignaling(args.signaling_file)
    frame_server = FrameSocketServer()
    await frame_server.start(args.frame_host, args.frame_port)

    pc = RTCPeerConnection()

    @pc.on("connectionstatechange")
    async def on_connectionstatechange() -> None:
        print("connection_state:", pc.connectionState)

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

    print("waiting_for_offer")
    offer_data = await wait_for_offer(signaling)
    offer = RTCSessionDescription(sdp=offer_data["sdp"], type=offer_data["type"])

    await pc.setRemoteDescription(offer)

    answer = await pc.createAnswer()
    await pc.setLocalDescription(answer)
    await wait_for_ice_gathering_complete(pc)

    signaling.write(
        "answer",
        {
            "sdp": pc.localDescription.sdp,
            "type": pc.localDescription.type,
        },
    )

    print("answer_written:", args.signaling_file)

    try:
        while True:
            await asyncio.sleep(1.0)
    except KeyboardInterrupt:
        pass
    finally:
        await pc.close()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--signaling-file", default="../webrtc_signaling/session.json")
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