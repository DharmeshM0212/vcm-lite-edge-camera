import argparse
import asyncio
import fractions
import time
from pathlib import Path

import cv2
from aiortc import RTCPeerConnection, RTCSessionDescription, VideoStreamTrack
from av import VideoFrame

from signaling import FileSignaling


async def wait_for_ice_gathering_complete(pc: RTCPeerConnection) -> None:
    if pc.iceGatheringState == "complete":
        return

    event = asyncio.Event()

    @pc.on("icegatheringstatechange")
    def on_icegatheringstatechange() -> None:
        print("ice_gathering_state:", pc.iceGatheringState)

        if pc.iceGatheringState == "complete":
            event.set()

    await event.wait()


class DatasetVideoTrack(VideoStreamTrack):
    def __init__(self, video_path: str, loop: bool) -> None:
        super().__init__()
        self.video_path = video_path
        self.loop = loop
        self.capture = cv2.VideoCapture(video_path)

        if not self.capture.isOpened():
            raise RuntimeError(f"failed_to_open_video:{video_path}")

        fps = self.capture.get(cv2.CAP_PROP_FPS)

        if fps <= 1.0 or fps > 240.0:
            fps = 30.0

        self.fps = fps
        self.frame_interval = 1.0 / fps
        self.frame_id = 0

    async def recv(self) -> VideoFrame:
        await asyncio.sleep(self.frame_interval)

        ok, frame = self.capture.read()

        if not ok:
            if not self.loop:
                raise asyncio.CancelledError()

            self.capture.set(cv2.CAP_PROP_POS_FRAMES, 0)
            ok, frame = self.capture.read()

            if not ok:
                raise asyncio.CancelledError()

        frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        video_frame = VideoFrame.from_ndarray(frame, format="rgb24")
        video_frame.pts = self.frame_id
        video_frame.time_base = fractions.Fraction(1, int(round(self.fps)))

        self.frame_id += 1
        return video_frame


async def wait_for_answer(signaling: FileSignaling) -> dict:
    while True:
        answer = signaling.read("answer")

        if answer is not None:
            return answer

        await asyncio.sleep(0.25)


async def run(args: argparse.Namespace) -> None:
    signaling_path = Path(args.signaling_file)

    if args.reset and signaling_path.exists():
        signaling_path.unlink()

    signaling = FileSignaling(args.signaling_file)

    pc = RTCPeerConnection()
    track = DatasetVideoTrack(args.video, args.loop)
    pc.addTrack(track)

    @pc.on("connectionstatechange")
    async def on_connectionstatechange() -> None:
        print("connection_state:", pc.connectionState)

    @pc.on("iceconnectionstatechange")
    async def on_iceconnectionstatechange() -> None:
        print("ice_connection_state:", pc.iceConnectionState)

    offer = await pc.createOffer()
    await pc.setLocalDescription(offer)
    await wait_for_ice_gathering_complete(pc)

    signaling.write(
        "offer",
        {
            "sdp": pc.localDescription.sdp,
            "type": pc.localDescription.type,
        },
    )

    print("offer_written:", args.signaling_file)
    print("waiting_for_answer")

    answer_data = await wait_for_answer(signaling)
    answer = RTCSessionDescription(sdp=answer_data["sdp"], type=answer_data["type"])
    await pc.setRemoteDescription(answer)

    print("streaming:", args.video)

    try:
        while True:
            await asyncio.sleep(1.0)
    except KeyboardInterrupt:
        pass
    finally:
        await pc.close()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--video", required=True)
    parser.add_argument("--signaling-file", default="../webrtc_signaling/session.json")
    parser.add_argument("--loop", action="store_true")
    parser.add_argument("--reset", action="store_true")
    return parser.parse_args()


def main() -> None:
    asyncio.run(run(parse_args()))


if __name__ == "__main__":
    main()