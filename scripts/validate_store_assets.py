#!/usr/bin/env python3
"""Validate the CardputerZero Store manifest and native PNG assets."""

from __future__ import annotations

import json
import struct
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def png_size(path: Path) -> tuple[int, int]:
    with path.open("rb") as stream:
        header = stream.read(24)
    if len(header) != 24 or header[:8] != b"\x89PNG\r\n\x1a\n" or header[12:16] != b"IHDR":
        raise ValueError(f"not a valid PNG: {path.relative_to(ROOT)}")
    return struct.unpack(">II", header[16:24])


def require_relative_file(value: object, field: str) -> Path:
    if not isinstance(value, str) or not value:
        raise ValueError(f"{field} must be a non-empty string")
    path = Path(value)
    if path.is_absolute() or ".." in path.parts:
        raise ValueError(f"{field} must stay inside the repository")
    resolved = ROOT / path
    if not resolved.is_file():
        raise ValueError(f"{field} does not exist: {value}")
    return resolved


def main() -> int:
    manifest_path = ROOT / "app-builder.json"
    data = json.loads(manifest_path.read_text(encoding="utf-8"))
    for field in ("package_name", "bin_name", "version", "app_name"):
        if not isinstance(data.get(field), str) or not data[field]:
            raise ValueError(f"missing required field: {field}")
    if data["package_name"] != "cardputerzero-sdr" or data["bin_name"] != "cardputerzero-sdr":
        raise ValueError("package_name and bin_name must match the installed app")

    store = data.get("store")
    if not isinstance(store, dict):
        raise ValueError("missing store object")
    screenshots = store.get("screenshots")
    if not isinstance(screenshots, list) or not screenshots:
        raise ValueError("store.screenshots must contain at least one path")
    for index, screenshot in enumerate(screenshots):
        path = require_relative_file(screenshot, f"store.screenshots[{index}]")
        if png_size(path) != (320, 170):
            raise ValueError(f"screenshot must be 320x170: {path.relative_to(ROOT)}")

    icon = require_relative_file(store.get("icon"), "store.icon")
    width, height = png_size(icon)
    if width != height or width < 100:
        raise ValueError("store.icon must be square and at least 100x100")

    permissions = store.get("permissions", [])
    if not isinstance(permissions, list) or not all(isinstance(item, str) for item in permissions):
        raise ValueError("store.permissions must be a string array")

    print(f"validated {len(screenshots)} screenshots, {width}x{height} icon, and store manifest")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"store validation failed: {error}", file=sys.stderr)
        raise SystemExit(1)
