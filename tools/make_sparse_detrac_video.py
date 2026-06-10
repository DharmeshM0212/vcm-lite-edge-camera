import argparse
from pathlib import Path

import cv2


def count_boxes(label_path: Path) -> int:
    if not label_path.exists():
        return 0

    lines = [x.strip() for x in label_path.read_text().splitlines() if x.strip()]
    return len(lines)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--dataset-root", required=True)
    parser.add_argument("--split", default="val")
    parser.add_argument("--output", required=True)
    parser.add_argument("--min-boxes", type=int, default=1)
    parser.add_argument("--max-boxes", type=int, default=2)
    parser.add_argument("--limit", type=int, default=900)
    parser.add_argument("--fps", type=float, default=15.0)
    parser.add_argument("--width", type=int, default=640)
    args = parser.parse_args()

    root = Path(args.dataset_root)
    image_dir = root / args.split / "images"
    label_dir = root / args.split / "labels"

    selected = []

    for image_path in sorted(image_dir.glob("*.jpg")):
        label_path = label_dir / f"{image_path.stem}.txt"
        n = count_boxes(label_path)

        if args.min_boxes <= n <= args.max_boxes:
            selected.append(image_path)

        if args.limit > 0 and len(selected) >= args.limit:
            break

    if not selected:
        raise RuntimeError("no sparse frames selected")

    first = cv2.imread(str(selected[0]))

    if first is None:
        raise RuntimeError("failed to read first image")

    h, w = first.shape[:2]
    scale = args.width / float(w)
    out_w = args.width
    out_h = int(h * scale)

    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)

    writer = cv2.VideoWriter(
        str(output_path),
        cv2.VideoWriter_fourcc(*"mp4v"),
        args.fps,
        (out_w, out_h)
    )

    for image_path in selected:
        frame = cv2.imread(str(image_path))

        if frame is None:
            continue

        frame = cv2.resize(frame, (out_w, out_h), interpolation=cv2.INTER_AREA)
        writer.write(frame)

    writer.release()

    print(f"saved:{output_path}")
    print(f"frames:{len(selected)}")
    print(f"box_range:{args.min_boxes}-{args.max_boxes}")
    print(f"size:{out_w}x{out_h}")


if __name__ == "__main__":
    main()