import argparse
import asyncio
import fractions
from pathlib import Path

import cv2
from aiortc import RTCPeerConnection, RTCSessionDescription, VideoStreamTrack
from av import VideoFrame

from http_signaling import HttpSignaling


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


def discover_videos(video_path: str | None, video_dir: str | None) -> list[Path]:
    if video_path:
        path = Path(video_path)
        if not path.exists():
            raise RuntimeError(f"video_not_found:{path}")
        return [path]

    if not video_dir:
        raise RuntimeError("either --video or --video-dir is required")

    directory = Path(video_dir)

    if not directory.exists() or not directory.is_dir():
        raise RuntimeError(f"video_dir_not_found:{directory}")

    extensions = {".mp4", ".mov", ".avi", ".mkv", ".webm"}
    videos = sorted([p for p in directory.iterdir() if p.suffix.lower() in extensions])

    if not videos:
        raise RuntimeError(f"no_videos_found_in:{directory}")

    return videos


def resize_frame(frame, resize_width: int) -> object:
    if resize_width <= 0:
        return frame

    height, width = frame.shape[:2]

    if width <= resize_width:
        return frame

    scale = resize_width / float(width)
    new_height = max(1, int(round(height * scale)))

    return cv2.resize(frame, (resize_width, new_height), interpolation=cv2.INTER_AREA)


class DatasetVideoTrack(VideoStreamTrack):
    def __init__(self, video_paths: list[Path], loop: bool, resize_width: int, max_fps: float) -> None:
        super().__init__()
        self.video_paths = video_paths
        self.loop = loop
        self.resize_width = resize_width
        self.max_fps = max_fps
        self.video_index = 0
        self.capture = None
        self.fps = 30.0
        self.frame_interval = 1.0 / self.fps
        self.frame_id = 0
        self.finished = False
        self.open_current_video()

    def open_current_video(self) -> None:
        if self.capture is not None:
            self.capture.release()
            self.capture = None

        if self.video_index >= len(self.video_paths):
            self.finished = True
            return

        current = self.video_paths[self.video_index]
        self.capture = cv2.VideoCapture(str(current))

        if not self.capture.isOpened():
            raise RuntimeError(f"failed_to_open_video:{current}")

        fps = self.capture.get(cv2.CAP_PROP_FPS)

        if fps <= 1.0 or fps > 240.0:
            fps = 30.0

        if self.max_fps > 0:
            fps = min(fps, self.max_fps)

        self.fps = fps
        self.frame_interval = 1.0 / fps

        print(f"playing_video:{self.video_index + 1}/{len(self.video_paths)}:{current}")
        print(f"sender_fps_limit:{self.fps:.2f}")
        if self.resize_width > 0:
            print(f"sender_resize_width:{self.resize_width}")

    def advance_video(self) -> None:
        self.video_index += 1

        if self.video_index >= len(self.video_paths):
            if self.loop:
                self.video_index = 0
            else:
                self.finished = True
                return

        self.open_current_video()

    async def recv(self) -> VideoFrame:
        if self.finished:
            raise asyncio.CancelledError()

        await asyncio.sleep(self.frame_interval)

        while True:
            if self.capture is None:
                self.open_current_video()

            ok, frame = self.capture.read()

            if ok:
                break

            self.advance_video()

            if self.finished:
                print("video_playlist_finished")
                raise asyncio.CancelledError()

        frame = resize_frame(frame, self.resize_width)
        frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)

        video_frame = VideoFrame.from_ndarray(frame, format="rgb24")
        video_frame.pts = self.frame_id
        video_frame.time_base = fractions.Fraction(1, max(1, int(round(self.fps))))

        self.frame_id += 1
        return video_frame


async def run(args: argparse.Namespace) -> None:
    video_paths = discover_videos(args.video, args.video_dir)

    print("playlist:")
    for index, path in enumerate(video_paths, start=1):
        print(f"  {index}. {path}")

    signaling = HttpSignaling(args.signaling_url)

    if args.reset:
        await signaling.reset()

    pc = RTCPeerConnection()
    track = DatasetVideoTrack(video_paths, args.loop, args.resize_width, args.max_fps)
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

    await signaling.write_offer(
        {
            "sdp": pc.localDescription.sdp,
            "type": pc.localDescription.type,
        }
    )

    print("offer_posted:", args.signaling_url)
    print("waiting_for_answer")

    answer_data = await signaling.wait_for_answer()
    answer = RTCSessionDescription(sdp=answer_data["sdp"], type=answer_data["type"])
    await pc.setRemoteDescription(answer)

    print("streaming_started")

    try:
        while not track.finished:
            await asyncio.sleep(1.0)

        print("sender_finished_all_videos")
    except KeyboardInterrupt:
        pass
    except asyncio.CancelledError:
        pass
    finally:
        await pc.close()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--video", default=None)
    parser.add_argument("--video-dir", default=None)
    parser.add_argument("--signaling-url", default="http://127.0.0.1:9000")
    parser.add_argument("--loop", action="store_true")
    parser.add_argument("--reset", action="store_true")
    parser.add_argument("--resize-width", type=int, default=640)
    parser.add_argument("--max-fps", type=float, default=12.0)
    return parser.parse_args()


def main() -> None:
    asyncio.run(run(parse_args()))


if __name__ == "__main__":
    main()