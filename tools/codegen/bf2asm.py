#!/usr/bin/env python3
"""Convert .bf source files to MASM assembly embedded in .text."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

BF_CHARS = set("+-<>[],.")
CHUNK_SIZE = 64
EXPORT_OUTPUT_DIRECTIVE = "bfdll: export=output"


def strip_comments(source: str) -> str:
    lines: list[str] = []
    for line in source.splitlines():
        if ";" in line:
            line = line[: line.index(";")]
        lines.append(line)
    return "\n".join(lines)


def strip_bf(source: str) -> str:
    return "".join(ch for ch in strip_comments(source) if ch in BF_CHARS)


def name_from_path(path: Path) -> str:
    return "BF_Prog_" + path.stem.replace("-", "_").title().replace("_", "")


def export_name_from_path(path: Path) -> str:
    return "BF_" + path.stem.replace("-", "_").title().replace("_", "")


def is_output_export(source: str) -> bool:
    return any(EXPORT_OUTPUT_DIRECTIVE in line for line in source.splitlines())


def emit_db_lines(code: str) -> list[str]:
    lines: list[str] = []
    for i in range(0, len(code), CHUNK_SIZE):
        chunk = code[i : i + CHUNK_SIZE]
        escaped = chunk.replace("'", "''")
        lines.append(f"    db '{escaped}'")
    lines.append("    db 0")
    return lines


def load_program(bf_path: Path) -> dict[str, object]:
    source = bf_path.read_text(encoding="utf-8")
    code = strip_bf(source)
    return {
        "path": bf_path,
        "source_name": bf_path.name,
        "source": source,
        "code": code,
        "program_symbol": name_from_path(bf_path),
        "export_symbol": export_name_from_path(bf_path) if is_output_export(source) else None,
        "core_pattern": code[:64] if len(code) >= 16 else code,
    }


def generate_program_block(program: dict[str, object]) -> str:
    bf_path = program["path"]
    code = program["code"]
    symbol = program["program_symbol"]

    body = "\n".join(emit_db_lines(code))
    return (
        f"; from {bf_path.name}\n"
        f"public {symbol}\n"
        f"{symbol} label byte\n"
        f"{body}\n"
    )


def generate_asm(programs: list[dict[str, object]]) -> str:
    blocks = [generate_program_block(program) for program in programs]
    return (
        "; AUTO-GENERATED - DO NOT EDIT\n"
        "bf_text segment 'CODE'\n"
        "align 16\n"
        "\n"
        + "\n".join(blocks)
        + "\nbf_text ends\nend\n"
    )


def generate_header(programs: list[dict[str, object]]) -> str:
    declarations: list[str] = []
    for program in programs:
        export_symbol = program["export_symbol"]
        if export_symbol:
            declarations.append(f"BFDLL_API const char* __cdecl {export_symbol}(void);")

    body = "\n".join(declarations) if declarations else "/* No generated exports. */"
    return (
        "/* AUTO-GENERATED - DO NOT EDIT */\n"
        "#ifndef BF_EXPORTS_GEN_H\n"
        "#define BF_EXPORTS_GEN_H\n"
        "\n"
        "#ifdef __cplusplus\n"
        "extern \"C\" {\n"
        "#endif\n"
        "\n"
        "#ifdef BFDLL_EXPORTS\n"
        "#define BFDLL_API __declspec(dllexport)\n"
        "#else\n"
        "#define BFDLL_API __declspec(dllimport)\n"
        "#endif\n"
        "\n"
        f"{body}\n"
        "\n"
        "#ifdef __cplusplus\n"
        "}\n"
        "#endif\n"
        "\n"
        "#endif\n"
    )


def generate_source(programs: list[dict[str, object]]) -> str:
    parts = [
        "/* AUTO-GENERATED - DO NOT EDIT */",
        '#include "bf_exports.gen.h"',
        '#include "bf_export_runtime.h"',
        "",
    ]

    for program in programs:
        export_symbol = program["export_symbol"]
        if not export_symbol:
            continue
        program_symbol = program["program_symbol"]
        parts.extend(
            [
                f"extern const char {program_symbol}[];",
                "",
                f"const char* __cdecl {export_symbol}(void)",
                "{",
                f"    return bfdll_run_output_program({program_symbol});",
                "}",
                "",
            ]
        )

    return "\n".join(parts)


def build_manifest(programs: list[dict[str, object]], dll_path: Path) -> dict[str, object]:
    runtime_exports = ["BF_GetLastOutput", "BF_SetOutputCallback"]
    generated_exports = [
        str(program["export_symbol"])
        for program in programs
        if program.get("export_symbol")
    ]
    return {
        "pe_path": str(dll_path.resolve()),
        "pe_kind": "dll",
        "exports": generated_exports + runtime_exports,
        "programs": [
            {
                "source": program["source_name"],
                "program_symbol": program["program_symbol"],
                "export_symbol": program["export_symbol"],
                "core_pattern": program["core_pattern"],
            }
            for program in programs
        ],
    }


def main() -> int:
    parser = argparse.ArgumentParser(description="Convert Brainfuck to MASM .text$bf assembly")
    parser.add_argument("inputs", nargs="+", type=Path, help=".bf source files")
    parser.add_argument("-o", "--output", type=Path, required=True, help="Output .asm path")
    parser.add_argument("--header", type=Path, help="Output generated C header path")
    parser.add_argument("--source", type=Path, help="Output generated C source path")
    parser.add_argument("--manifest", type=Path, help="Output build manifest JSON path")
    parser.add_argument("--dll-path", type=Path, help="Expected DLL path for manifest")
    args = parser.parse_args()

    programs: list[dict[str, object]] = []
    for bf_path in args.inputs:
        if not bf_path.is_file():
            print(f"error: {bf_path} not found", file=sys.stderr)
            return 1
        program = load_program(bf_path)
        if not program["export_symbol"]:
            print(
                f"error: {bf_path} missing '; bfdll: export=output' (Phase 0 export directive)",
                file=sys.stderr,
            )
            return 1
        programs.append(program)

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(generate_asm(programs), encoding="utf-8")

    if args.header:
        args.header.parent.mkdir(parents=True, exist_ok=True)
        args.header.write_text(generate_header(programs), encoding="utf-8")
    if args.source:
        args.source.parent.mkdir(parents=True, exist_ok=True)
        args.source.write_text(generate_source(programs), encoding="utf-8")
    if args.manifest:
        dll_path = args.dll_path or Path("out.dll")
        args.manifest.parent.mkdir(parents=True, exist_ok=True)
        args.manifest.write_text(
            json.dumps(build_manifest(programs, dll_path), indent=2),
            encoding="utf-8",
        )

    print(f"Generated {args.output} from {len(args.inputs)} program(s)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
