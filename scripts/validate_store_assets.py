#!/usr/bin/env python3
"""Validate the CardputerZero Store manifest and native PNG assets."""

from __future__ import annotations

import json
import re
import struct
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
EXPECTED_LOCALES = {
    "en",
    "zh-CN",
    "zh-TW",
    "es",
    "ja",
    "ko",
    "fr",
    "de",
    "pt-BR",
    "ru",
}
EXPECTED_PERMISSIONS = {
    "microphone": False,
    "audio_output": True,
    "network": False,
    "filesystem": "app-data-only",
    "keyboard_input": True,
    "background_service": False,
    "external_hardware": True,
    "hdmi_output": False,
    "camera": False,
    "sensors": False,
    "gps": False,
    "device_id": False,
}
EXPECTED_SHARE_CODE = "zsdr"


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


def require_text(value: object, field: str) -> str:
    if not isinstance(value, str) or not value.strip():
        raise ValueError(f"{field} must be a non-empty string")
    return value


def main() -> int:
    manifest_path = ROOT / "app-builder.json"
    data = json.loads(manifest_path.read_text(encoding="utf-8"))
    for field in ("package_name", "bin_name", "version", "app_name"):
        if not isinstance(data.get(field), str) or not data[field]:
            raise ValueError(f"missing required field: {field}")
    if data["package_name"] != "cardputerzero-sdr" or data["bin_name"] != "cardputerzero-sdr":
        raise ValueError("package_name and bin_name must match the installed app")
    if data["app_name"] != "Zero SDR Keyboard":
        raise ValueError("app_name must distinguish this app from the existing zerosdr listing")

    store = data.get("store")
    if not isinstance(store, dict):
        raise ValueError("missing store object")
    share_code = store.get("share_code")
    if not isinstance(share_code, str) or not re.fullmatch(r"[a-z0-9]{4,8}", share_code):
        raise ValueError("store.share_code must contain 4-8 lowercase ASCII letters or digits")
    if share_code != EXPECTED_SHARE_CODE:
        raise ValueError(
            f"store.share_code must remain the reserved project code: {EXPECTED_SHARE_CODE}"
        )
    require_text(store.get("summary"), "store.summary")
    require_text(store.get("description"), "store.description")
    categories = store.get("categories")
    if not isinstance(categories, list) or not categories or not all(
        isinstance(item, str) and item for item in categories
    ):
        raise ValueError("store.categories must be a non-empty string array")
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

    permissions = store.get("permissions")
    if permissions != EXPECTED_PERMISSIONS:
        raise ValueError(
            "store.permissions must be the complete registry permission object "
            "for this offline keyboard/audio/RTL-SDR app"
        )

    locales = store.get("locales")
    if not isinstance(locales, dict) or set(locales) != EXPECTED_LOCALES:
        raise ValueError("store.locales must cover the same ten locales as the app")
    for locale, localized in locales.items():
        if not isinstance(localized, dict):
            raise ValueError(f"store.locales.{locale} must be an object")
        for field in ("title", "summary", "description"):
            require_text(localized.get(field), f"store.locales.{locale}.{field}")
        if localized["title"].casefold().replace(" ", "") == "zerosdr":
            raise ValueError(f"store.locales.{locale}.title collides with the existing zerosdr listing")

    print(
        f"validated {len(screenshots)} screenshots, {width}x{height} icon, "
        f"{len(locales)} locales, share code {share_code}, and explicit registry permissions"
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"store validation failed: {error}", file=sys.stderr)
        raise SystemExit(1)
