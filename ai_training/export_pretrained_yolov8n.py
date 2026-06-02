from pathlib import Path
from shutil import copyfile
from ultralytics import YOLO


ROOT = Path(__file__).resolve().parent
PROJECT_ROOT = ROOT.parent
MODELS_DIR = PROJECT_ROOT / "models"


def main() -> None:
    MODELS_DIR.mkdir(parents=True, exist_ok=True)

    model = YOLO("yolov8n.pt")

    exported_path = model.export(
        format="onnx",
        imgsz=320,
        opset=12,
        simplify=True,
        dynamic=False
    )

    output_path = MODELS_DIR / "object_detector.onnx"
    copyfile(exported_path, output_path)

    print(output_path)


if __name__ == "__main__":
    main()