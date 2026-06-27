#!/usr/bin/env python3
"""Generate C++ sources embedding runtime/ and tools/verify_pe.ps1 for standalone bfpe.exe."""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path

EMBED_PATHS = [
    "runtime/bf_abi.h",
    "runtime/bf_export_runtime.c",
    "runtime/bf_export_runtime.h",
    "runtime/bf_io.c",
    "runtime/bf_io.h",
    "runtime/bf_stub.c",
    "runtime/dllmain.c",
    "runtime/vm/bf_vm.c",
    "runtime/vm/bf_vm.h",
    "tools/verify_pe.ps1",
]

BUNDLE_VERSION = "1"


def c_identifier(relative_path: str) -> str:
    return "k_embed_" + relative_path.replace("/", "_").replace(".", "_").replace("-", "_")


def emit_byte_array(name: str, data: bytes) -> list[str]:
    lines = [f"static const unsigned char {name}[] = {{"]
    row: list[str] = []
    for index, byte in enumerate(data):
        row.append(f"0x{byte:02x}")
        if len(row) == 16:
            lines.append("    " + ", ".join(row) + ",")
            row = []
    if row:
        lines.append("    " + ", ".join(row) + ",")
    lines.append("};")
    lines.append(f"static const size_t {name}_size = {len(data)};")
    return lines


def generate(root: Path, out_dir: Path) -> None:
    out_dir.mkdir(parents=True, exist_ok=True)
    header_path = out_dir / "embedded_bundle.gen.hpp"
    source_path = out_dir / "embedded_bundle.gen.cpp"

    entries: list[tuple[str, bytes, str]] = []
    digest = hashlib.sha256()
    for relative in EMBED_PATHS:
        path = root / relative
        data = path.read_bytes()
        digest.update(relative.encode("utf-8"))
        digest.update(b"\0")
        digest.update(data)
        entries.append((relative.replace("\\", "/"), data, c_identifier(relative)))

    fingerprint = digest.hexdigest()[:16]

    header_lines = [
        "#pragma once",
        "",
        "#include <cstddef>",
        "",
        "struct EmbeddedBundleFile {",
        "    const char* relative_path;",
        "    const unsigned char* data;",
        "    size_t size;",
        "};",
        "",
        f'inline constexpr const char kEmbeddedBundleVersion[] = "{BUNDLE_VERSION}";',
        f'inline constexpr const char kEmbeddedBundleFingerprint[] = "{fingerprint}";',
        "extern const EmbeddedBundleFile kEmbeddedBundleFiles[];",
        "extern const size_t kEmbeddedBundleFileCount;",
        "",
    ]

    source_lines = [
        '#include "embedded_bundle.gen.hpp"',
        "",
    ]

    for relative, data, ident in entries:
        source_lines.extend(emit_byte_array(ident, data))
        source_lines.append("")

    source_lines.append("const EmbeddedBundleFile kEmbeddedBundleFiles[] = {")
    for relative, _, ident in entries:
        source_lines.append(
            f'    {{"{relative}", {ident}, {ident}_size}},'
        )
    source_lines.append("};")
    source_lines.append(f"const size_t kEmbeddedBundleFileCount = {len(entries)};")
    source_lines.append("")

    header_path.write_text("\n".join(header_lines), encoding="utf-8")
    source_path.write_text("\n".join(source_lines), encoding="utf-8")
    print(f"Generated {source_path} ({len(entries)} files, fingerprint {fingerprint})")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--out-dir", type=Path, required=True)
    args = parser.parse_args()
    generate(args.root.resolve(), args.out_dir.resolve())


if __name__ == "__main__":
    main()
