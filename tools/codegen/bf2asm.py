#!/usr/bin/env python3
"""Convert .bf source files to MASM assembly embedded in .text."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

from parse_sig import Signature, parse_file

BF_CHARS = set("+-<>[],.")
CHUNK_SIZE = 64


def strip_comments(source: str) -> str:
    lines: list[str] = []
    for line in source.splitlines():
        if ";" in line:
            line = line[: line.index(";")]
        lines.append(line)
    return "\n".join(lines)


def strip_bf(source: str) -> str:
    return "".join(ch for ch in strip_comments(source) if ch in BF_CHARS)


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
    sig = parse_file(bf_path)
    code = strip_bf(source)
    return {
        "path": bf_path,
        "source_name": bf_path.name,
        "source": source,
        "code": code,
        "signature": sig,
        "program_symbol": sig.program_symbol,
        "export_symbol": sig.export_symbol,
        "core_pattern": code[:64] if len(code) >= 16 else code,
    }


def validate_export_uniqueness(programs: list[dict[str, object]]) -> None:
    by_name: dict[str, str] = {}
    by_symbol: dict[str, str] = {}
    for program in programs:
        sig: Signature = program["signature"]
        source = str(program["source_name"])
        key = sig.export_name.lower()
        if key in by_name:
            raise ValueError(
                f"duplicate export name {sig.export_name!r} in {source} and {by_name[key]}"
            )
        by_name[key] = source
        if sig.export_symbol in by_symbol:
            raise ValueError(
                f"duplicate export symbol {sig.export_symbol} in {source} and {by_symbol[sig.export_symbol]}"
            )
        by_symbol[sig.export_symbol] = source


def load_programs(bf_paths: list[Path]) -> list[dict[str, object]]:
    programs: list[dict[str, object]] = []
    for bf_path in bf_paths:
        if not bf_path.is_file():
            raise FileNotFoundError(f"{bf_path} not found")
        programs.append(load_program(bf_path))
    validate_export_uniqueness(programs)
    return programs


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


def c_return_type(sig: Signature) -> str:
    if sig.return_type == "const char*":
        return "const char*"
    return sig.return_type


def generate_header_decl(sig: Signature) -> str:
    params = ", ".join(f"int {param.name}" for param in sig.params)
    return f"BFDLL_API {c_return_type(sig)} __cdecl {sig.export_symbol}({params})"


def generate_export_body(sig: Signature, program_symbol: str) -> list[str]:
    io_mode = sig.io_mode_c
    if sig.return_type == "int":
        if sig.params:
            arg_names = ", ".join(param.name for param in sig.params)
            return [
                f"int __cdecl {sig.export_symbol}({', '.join(f'int {p.name}' for p in sig.params)})",
                "{",
                f"    const int args[] = {{ {arg_names} }};",
                f"    return bfpe_run_int_program({program_symbol}, {io_mode}, args, {len(sig.params)});",
                "}",
            ]
        return [
            f"int __cdecl {sig.export_symbol}(void)",
            "{",
            f"    return bfpe_run_int_program({program_symbol}, {io_mode}, NULL, 0);",
            "}",
        ]

    if sig.return_type == "const char*":
        return [
            f"const char* __cdecl {sig.export_symbol}(void)",
            "{",
            f"    return bfpe_run_string_program({program_symbol}, {io_mode});",
            "}",
        ]

    return [
        f"void __cdecl {sig.export_symbol}(void)",
        "{",
        f"    bfpe_run_void_program({program_symbol}, {io_mode});",
        "}",
    ]


def generate_header(programs: list[dict[str, object]], *, pe_kind: str) -> str:
    sigs = [program["signature"] for program in programs]
    declarations = [generate_header_decl(sig) + ";" for sig in sigs]
    body = "\n".join(declarations) if declarations else "/* No generated exports. */"
    api_block = (
        "#define BFDLL_API\n"
        if pe_kind == "exe"
        else "#ifdef BFDLL_EXPORTS\n"
        "#define BFDLL_API __declspec(dllexport)\n"
        "#else\n"
        "#define BFDLL_API __declspec(dllimport)\n"
        "#endif\n"
    )
    return (
        "/* AUTO-GENERATED - DO NOT EDIT */\n"
        "#ifndef BF_EXPORTS_GEN_H\n"
        "#define BF_EXPORTS_GEN_H\n"
        "\n"
        "#ifdef __cplusplus\n"
        "extern \"C\" {\n"
        "#endif\n"
        "\n"
        f"{api_block}\n"
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
        sig: Signature = program["signature"]
        program_symbol = program["program_symbol"]
        parts.append(f"extern const char {program_symbol}[];")
        parts.append("")
        parts.extend(generate_export_body(sig, program_symbol))
        parts.append("")

    return "\n".join(parts)


def generate_exe_main(sig: Signature) -> str:
    parts = [
        "/* AUTO-GENERATED - DO NOT EDIT */",
        "#include <stdio.h>",
        "#include <stdlib.h>",
        '#include "bf_exports.gen.h"',
        "",
    ]

    if sig.return_type == "int":
        parts.append("int main(int argc, char** argv)")
        parts.append("{")
        if sig.params:
            expected = len(sig.params) + 1
            parts.append(f"    if (argc < {expected}) {{")
            parts.append(
                f'        fprintf(stderr, "usage: %s {" ".join(p.name for p in sig.params)}\\n", argv[0]);'
            )
            parts.append("        return 1;")
            parts.append("    }")
            for index, param in enumerate(sig.params, start=1):
                parts.append(f"    int {param.name} = atoi(argv[{index}]);")
            arg_list = ", ".join(param.name for param in sig.params)
            parts.append(f"    printf(\"%d\\n\", {sig.export_symbol}({arg_list}));")
        else:
            parts.append(f"    printf(\"%d\\n\", {sig.export_symbol}());")
        parts.extend(["    return 0;", "}"])
        return "\n".join(parts)

    if sig.return_type == "const char*":
        parts.extend(
            [
                "int main(void)",
                "{",
                f"    const char* out = {sig.export_symbol}();",
                '    if (out && out[0]) {',
                '        printf("%s\\n", out);',
                "    }",
                "    return 0;",
                "}",
            ]
        )
        return "\n".join(parts)

    parts.extend(
        [
            "int main(void)",
            "{",
            f"    {sig.export_symbol}();",
            "    return 0;",
            "}",
        ]
    )
    return "\n".join(parts)


def build_manifest(
    programs: list[dict[str, object]], pe_path: Path, *, pe_kind: str
) -> dict[str, object]:
    runtime_exports = ["BF_GetLastOutput", "BF_SetOutputCallback"]
    generated_exports = [str(program["export_symbol"]) for program in programs]
    exports = generated_exports + (runtime_exports if pe_kind == "dll" else [])
    return {
        "pe_path": str(pe_path.resolve()),
        "pe_kind": pe_kind,
        "exports": exports,
        "programs": [
            {
                "source": program["source_name"],
                "program_symbol": program["program_symbol"],
                "export_name": program["signature"].export_name,
                "export_symbol": program["export_symbol"],
                "core_pattern": program["core_pattern"],
                "return_type": program["signature"].return_type,
                "io_mode": program["signature"].io_mode,
                "is_entry": program["signature"].is_entry,
                "param_count": len(program["signature"].params),
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
    parser.add_argument("--pe-path", type=Path, help="Expected PE output path for manifest")
    parser.add_argument("--dll-path", type=Path, help=argparse.SUPPRESS)
    parser.add_argument(
        "--pe-kind",
        choices=("dll", "exe"),
        default="dll",
        help="PE kind for manifest/header generation",
    )
    parser.add_argument("--exe-main", type=Path, help="Output generated EXE main path")
    args = parser.parse_args()

    programs: list[dict[str, object]] = []
    try:
        programs = load_programs(list(args.inputs))
    except (FileNotFoundError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(generate_asm(programs), encoding="utf-8")

    if args.header:
        args.header.parent.mkdir(parents=True, exist_ok=True)
        args.header.write_text(generate_header(programs, pe_kind=args.pe_kind), encoding="utf-8")
    if args.source:
        args.source.parent.mkdir(parents=True, exist_ok=True)
        args.source.write_text(generate_source(programs), encoding="utf-8")
    if args.exe_main:
        if len(programs) != 1:
            print("error: EXE main generation requires exactly one .bf input", file=sys.stderr)
            return 1
        sig: Signature = programs[0]["signature"]
        args.exe_main.parent.mkdir(parents=True, exist_ok=True)
        args.exe_main.write_text(generate_exe_main(sig), encoding="utf-8")
    if args.manifest:
        pe_path = args.pe_path or args.dll_path or Path("out.dll")
        args.manifest.parent.mkdir(parents=True, exist_ok=True)
        args.manifest.write_text(
            json.dumps(build_manifest(programs, pe_path, pe_kind=args.pe_kind), indent=2),
            encoding="utf-8",
        )

    print(f"Generated {args.output} from {len(args.inputs)} program(s)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
