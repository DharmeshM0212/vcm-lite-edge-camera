import json
from pathlib import Path
from typing import Any


class FileSignaling:
    def __init__(self, path: str) -> None:
        self.path = Path(path)
        self.path.parent.mkdir(parents=True, exist_ok=True)

    def write(self, key: str, value: dict[str, Any]) -> None:
        data = {}

        if self.path.exists():
            try:
                data = json.loads(self.path.read_text(encoding="utf-8"))
            except json.JSONDecodeError:
                data = {}

        data[key] = value
        self.path.write_text(json.dumps(data, indent=2), encoding="utf-8")

    def read(self, key: str) -> dict[str, Any] | None:
        if not self.path.exists():
            return None

        try:
            data = json.loads(self.path.read_text(encoding="utf-8"))
        except json.JSONDecodeError:
            return None

        value = data.get(key)

        if isinstance(value, dict):
            return value

        return None