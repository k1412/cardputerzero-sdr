#!/usr/bin/env python3
"""Build small Noto UI font subsets for the ten-language interface.

This script is a maintainer tool. Generated fonts are committed so end users and
cross-build jobs do not need fontTools or system-wide Noto packages.
"""

from __future__ import annotations

import argparse
import ast
import re
from pathlib import Path

from fontTools import subset
from fontTools.varLib.instancer import instantiateVariableFont


ASCII_UI = (
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"
    " .,:;!?/+-()[]%<>"
)


def translation_text(source: Path) -> str:
    strings: list[str] = []
    for token in re.findall(r'"(?:\\.|[^"\\])*"', source.read_text(encoding="utf-8")):
        try:
            value = ast.literal_eval(token)
        except (SyntaxError, ValueError):
            continue
        if isinstance(value, str):
            strings.append(value)
    return ASCII_UI + "".join(strings)


def build_subset(source: Path, destination: Path, text: str, font_number: int | None = None) -> None:
    options = subset.Options()
    options.layout_features = ["*"]
    options.name_IDs = [0, 1, 2, 3, 4, 5, 6]
    options.name_legacy = True
    options.name_languages = [0x409]
    options.recalc_bounds = True
    options.recalc_timestamp = False
    if font_number is not None:
        options.font_number = font_number

    font = subset.load_font(str(source), options)
    if "fvar" in font:
        # Freeze Google Fonts variable sources at Regular.  Besides producing a
        # smaller file, this keeps the embedded LVGL/FreeType path simple.
        instantiateVariableFont(font, {"wght": 400}, inplace=True, updateFontNames=True)
    worker = subset.Subsetter(options=options)
    worker.populate(text=text)
    worker.subset(font)
    destination.parent.mkdir(parents=True, exist_ok=True)
    subset.save_font(font, str(destination), options)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--noto-sans",
        type=Path,
        default=Path("/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf"),
    )
    parser.add_argument(
        "--noto-cjk",
        type=Path,
        default=Path("/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc"),
    )
    parser.add_argument("--noto-jp", type=Path)
    parser.add_argument("--noto-kr", type=Path)
    parser.add_argument("--noto-sc", type=Path)
    parser.add_argument("--noto-tc", type=Path)
    parser.add_argument("--output", type=Path, default=Path("assets/fonts"))
    args = parser.parse_args()

    separate_cjk = (args.noto_jp, args.noto_kr, args.noto_sc, args.noto_tc)
    if any(separate_cjk) and not all(separate_cjk):
        raise SystemExit("pass all four of --noto-jp/kr/sc/tc, or none of them")

    sources = (args.noto_sans,) + (separate_cjk if all(separate_cjk) else (args.noto_cjk,))
    for source in sources:
        if not source.is_file():
            raise SystemExit(f"missing source font: {source}")

    text = translation_text(Path("src/i18n/translations.cpp"))
    build_subset(args.noto_sans, args.output / "noto-ui.ttf", text)
    if all(separate_cjk):
        cjk_sources = zip(
            ("noto-cjk-jp-ui.ttf", "noto-cjk-kr-ui.ttf", "noto-cjk-sc-ui.ttf", "noto-cjk-tc-ui.ttf"),
            separate_cjk,
        )
        for filename, source in cjk_sources:
            build_subset(source, args.output / filename, text)
    else:
        for filename, font_number in (
            ("noto-cjk-jp-ui.ttf", 0),
            ("noto-cjk-kr-ui.ttf", 1),
            ("noto-cjk-sc-ui.ttf", 2),
            ("noto-cjk-tc-ui.ttf", 3),
        ):
            build_subset(args.noto_cjk, args.output / filename, text, font_number)


if __name__ == "__main__":
    main()
