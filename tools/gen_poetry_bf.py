#!/usr/bin/env python3
"""Generate a bfpe .bf that prints text one ASCII char per tape cell."""

from __future__ import annotations

import argparse
from pathlib import Path

DEFAULT_POEM = """+-------------------------------------------------------+
|              THE DIGITAL STREAM                       |
+-------------------------------------------------------+
|  A lonely light upon the screen,                      |
|  A silent world of blue and green.                    |
|  The cursor blinks, a steady beat,                    |
|  Where logic and the spirit meet.                     |
|                                                       |
|  0101  The code is spun,                              |
|  A digital web under the sun.                         |
|  Yet in this grid of text and line,                   |
|  A human heart begins to shine.                       |
+-------------------------------------------------------+
|  Through copper veins the currents race,              |
|  Across the void of time and space.                   |
|  A thousand thoughts in packets fly,                  |
|  Beneath a cold, electric sky.                        |
|                                                       |
|  The database is deep and wide,                       |
|  Where secrets of the ages hide.                      |
|  But algorithms cannot know                           |
|  The reason why the tear-drops flow.                  |
+-------------------------------------------------------+
|  So let the keys continue ringing,                    |
|  A modern song of science singing.                    |
|  For even in the cold machine,                        |
|  The soul remains, alive, unseen.                     |
+-------------------------------------------------------+
"""


def emit_char(ch: str, first: bool) -> str:
    code = "" if first else ">"
    code += "+" * ord(ch)
    code += "."
    return code


def text_to_brainfuck(text: str) -> str:
    parts: list[str] = []
    for index, ch in enumerate(text):
        parts.append(emit_char(ch, first=(index == 0)))
    return "".join(parts)


def wrap_bfpe(text: str, export: str, fn: str) -> str:
    body = text_to_brainfuck(text)
    header = (
        f"; bfpe: export={export}\n"
        f"; bfpe: const char* {fn}(void)\n"
        f"; bfpe: io=buffer\n"
    )
    return header + body + "\n"


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate poetry.bf from ASCII text")
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        default=Path("examples/poetry.bf"),
        help="Output .bf path",
    )
    parser.add_argument(
        "-i",
        "--input",
        type=Path,
        help="Optional text file (default: built-in poem)",
    )
    parser.add_argument("--export", default="Poetry", help="bfpe export name")
    parser.add_argument("--fn", default="poetry", help="C function name in signature")
    args = parser.parse_args()

    text = args.input.read_text(encoding="utf-8") if args.input else DEFAULT_POEM
    if not text.endswith("\n"):
        text += "\n"

    out = wrap_bfpe(text, args.export, args.fn)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(out, encoding="utf-8")

    bf_len = len(text_to_brainfuck(text))
    print(f"Wrote {args.output} ({len(out)} bytes, bf body {bf_len} chars, text {len(text)} chars)")


if __name__ == "__main__":
    main()
