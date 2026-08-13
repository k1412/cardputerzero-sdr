#!/usr/bin/env python3
"""Reject CFF font assets that are unsafe in the target LVGL/FreeType path."""

from __future__ import annotations

import struct
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
FONT_DIR = ROOT / "assets" / "fonts"
CJK_FONTS = (
    "noto-cjk-jp-ui.ttf",
    "noto-cjk-kr-ui.ttf",
    "noto-cjk-sc-ui.ttf",
    "noto-cjk-tc-ui.ttf",
)


def table_tags(path: Path) -> set[bytes]:
    data = path.read_bytes()
    if len(data) < 12:
        raise ValueError(f"truncated font: {path.relative_to(ROOT)}")
    sfnt_version, table_count = struct.unpack_from(">IH", data)
    if sfnt_version not in (0x00010000, 0x74727565):
        raise ValueError(f"not a TrueType sfnt: {path.relative_to(ROOT)}")
    directory_end = 12 + table_count * 16
    if directory_end > len(data):
        raise ValueError(f"truncated table directory: {path.relative_to(ROOT)}")
    return {data[12 + index * 16 : 16 + index * 16] for index in range(table_count)}


def main() -> None:
    for filename in CJK_FONTS:
        path = FONT_DIR / filename
        tags = table_tags(path)
        if b"glyf" not in tags or b"CFF " in tags or b"CFF2" in tags:
            raise ValueError(f"CJK UI font must use glyf outlines: {path.relative_to(ROOT)}")
        print(f"validated {path.relative_to(ROOT)} ({path.stat().st_size} bytes, glyf)")


if __name__ == "__main__":
    main()
