import argparse
import shutil
import xml.etree.ElementTree as ET
from pathlib import Path

import cv2


CLASS_NAMES = ["car", "van", "bus", "others"]
CLASS_TO_ID = {name: i for i, name in enumerate(CLASS_NAMES)}


def find_nested_dir(root: Path, name: str) -> Path:
    direct = root / name

    if direct.exists():
        nested = direct / name

        if nested.exists():
            return nested

        return direct

    matches = [p for p in root.rglob(name) if p.is_dir()]

    if not matches:
        raise FileNotFoundError(f"Could not find folder: {name}")

    matches.sort(key=lambda p: len(str(p)))
    return matches[0]


def parse_vehicle_type(target) -> str:
    attribute = target.find("attribute")

    if attribute is None:
        return "others"

    vehicle_type = attribute.attrib.get("vehicle_type", "others").strip().lower()

    if vehicle_type in CLASS_TO_ID:
        return vehicle_type

    if vehicle_type in {"car.", "cars"}:
        return "car"

    if vehicle_type in {"van.", "minivan"}:
        return "van"

    if vehicle_type in {"bus.", "buses"}:
        return "bus"

    return "others"


def image_size(image_path: Path) -> tuple[int, int] | None:
    image = cv2.imread(str(image_path))

    if image is None:
        return None

    height, width = image.shape[:2]
    return width, height


def clamp(value: float, low: float, high: float) -> float:
    return max(low, min(high, value))


def convert_box_to_yolo(left: float, top: float, width: float, height: float, image_width: int, image_height: int) -> tuple[float, float, float, float] | None:
    x1 = clamp(left, 0.0, float(image_width - 1))
    y1 = clamp(top, 0.0, float(image_height - 1))
    x2 = clamp(left + width, 0.0, float(image_width - 1))
    y2 = clamp(top + height, 0.0, float(image_height - 1))

    box_width = x2 - x1
    box_height = y2 - y1

    if box_width <= 1.0 or box_height <= 1.0:
        return None

    x_center = (x1 + x2) / 2.0 / float(image_width)
    y_center = (y1 + y2) / 2.0 / float(image_height)
    norm_width = box_width / float(image_width)
    norm_height = box_height / float(image_height)

    return x_center, y_center, norm_width, norm_height


def frame_image_name(frame_num: int) -> str:
    return f"img{frame_num:05d}.jpg"


def convert_split(images_root: Path, xml_root: Path, output_root: Path, split_name: str, frame_stride: int, max_frames_per_sequence: int) -> dict:
    output_images = output_root / split_name / "images"
    output_labels = output_root / split_name / "labels"

    output_images.mkdir(parents=True, exist_ok=True)
    output_labels.mkdir(parents=True, exist_ok=True)

    stats = {
        "sequences": 0,
        "frames_seen": 0,
        "frames_saved": 0,
        "boxes_saved": 0,
        "missing_images": 0,
        "empty_labels": 0,
    }

    xml_files = sorted(xml_root.glob("*.xml"))

    for xml_path in xml_files:
        sequence_name = xml_path.stem
        sequence_image_dir = images_root / sequence_name

        if not sequence_image_dir.exists():
            continue

        tree = ET.parse(xml_path)
        root = tree.getroot()

        stats["sequences"] += 1
        saved_in_sequence = 0

        for frame in root.findall("frame"):
            frame_num_text = frame.attrib.get("num")

            if frame_num_text is None:
                continue

            frame_num = int(frame_num_text)
            stats["frames_seen"] += 1

            if frame_num % frame_stride != 0:
                continue

            if max_frames_per_sequence > 0 and saved_in_sequence >= max_frames_per_sequence:
                continue

            source_image = sequence_image_dir / frame_image_name(frame_num)

            if not source_image.exists():
                stats["missing_images"] += 1
                continue

            size = image_size(source_image)

            if size is None:
                stats["missing_images"] += 1
                continue

            image_width, image_height = size
            label_lines = []

            target_list = frame.find("target_list")

            if target_list is not None:
                for target in target_list.findall("target"):
                    box = target.find("box")

                    if box is None:
                        continue

                    vehicle_type = parse_vehicle_type(target)
                    class_id = CLASS_TO_ID[vehicle_type]

                    left = float(box.attrib.get("left", "0"))
                    top = float(box.attrib.get("top", "0"))
                    box_width = float(box.attrib.get("width", "0"))
                    box_height = float(box.attrib.get("height", "0"))

                    yolo_box = convert_box_to_yolo(left, top, box_width, box_height, image_width, image_height)

                    if yolo_box is None:
                        continue

                    x_center, y_center, norm_width, norm_height = yolo_box
                    label_lines.append(f"{class_id} {x_center:.6f} {y_center:.6f} {norm_width:.6f} {norm_height:.6f}")

            if not label_lines:
                stats["empty_labels"] += 1
                continue

            output_name = f"{sequence_name}_{source_image.name}"
            output_image = output_images / output_name
            output_label = output_labels / f"{Path(output_name).stem}.txt"

            shutil.copy2(source_image, output_image)
            output_label.write_text("\n".join(label_lines) + "\n", encoding="utf-8")

            stats["frames_saved"] += 1
            stats["boxes_saved"] += len(label_lines)
            saved_in_sequence += 1

    return stats


def write_data_yaml(output_root: Path) -> None:
    names_text = "\n".join([f"  {i}: {name}" for i, name in enumerate(CLASS_NAMES)])

    data_yaml = f"""path: {output_root.as_posix()}
train: train/images
val: val/images

names:
{names_text}
"""

    (output_root / "data.yaml").write_text(data_yaml, encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--detrac-root", required=True)
    parser.add_argument("--output-root", required=True)
    parser.add_argument("--train-stride", type=int, default=5)
    parser.add_argument("--val-stride", type=int, default=10)
    parser.add_argument("--max-train-frames-per-seq", type=int, default=350)
    parser.add_argument("--max-val-frames-per-seq", type=int, default=120)
    args = parser.parse_args()

    detrac_root = Path(args.detrac_root)
    output_root = Path(args.output_root)

    images_root = find_nested_dir(detrac_root, "DETRAC-Images")
    train_xml_root = find_nested_dir(detrac_root, "DETRAC-Train-Annotations-XML")
    test_xml_root = find_nested_dir(detrac_root, "DETRAC-Test-Annotations-XML")

    if output_root.exists():
        shutil.rmtree(output_root)

    output_root.mkdir(parents=True, exist_ok=True)

    train_stats = convert_split(
        images_root,
        train_xml_root,
        output_root,
        "train",
        args.train_stride,
        args.max_train_frames_per_seq,
    )

    val_stats = convert_split(
        images_root,
        test_xml_root,
        output_root,
        "val",
        args.val_stride,
        args.max_val_frames_per_seq,
    )

    write_data_yaml(output_root)

    print("images_root:", images_root)
    print("train_xml_root:", train_xml_root)
    print("test_xml_root:", test_xml_root)
    print("output_root:", output_root)
    print("classes:", CLASS_NAMES)
    print("train_stats:", train_stats)
    print("val_stats:", val_stats)
    print("data_yaml:", output_root / "data.yaml")


if __name__ == "__main__":
    main()