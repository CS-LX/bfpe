#!/usr/bin/env python3
"""Load and invoke BFPE-built PE files."""

from __future__ import annotations

import ctypes
import json
import subprocess
from pathlib import Path


def find_program(manifest: dict[str, object], export_name: str) -> dict[str, object]:
    programs = manifest.get("programs", [])
    if not programs:
        raise ValueError("manifest has no programs")

    matches = [
        program
        for program in programs
        if str(program.get("export_name", "")).lower() == export_name.lower()
    ]
    if not matches:
        known = ", ".join(str(program.get("export_name")) for program in programs)
        raise ValueError(f"export {export_name!r} not found (available: {known})")
    return matches[0]


def manifest_path_for_pe(root: Path, pe_path: Path) -> Path:
    return root / ".bfpe-build" / pe_path.stem / "manifest.json"


def load_manifest(root: Path, pe_path: Path) -> dict[str, object]:
    path = manifest_path_for_pe(root, pe_path)
    if not path.is_file():
        raise ValueError(
            f"manifest not found: {path} (rebuild with bfpe build ... -o {pe_path.name})"
        )
    return json.loads(path.read_text(encoding="utf-8"))


def run_pe(root: Path, pe_path: Path, export_name: str, run_args: list[str]) -> int:
    pe_path = pe_path.resolve()
    if not pe_path.is_file():
        raise FileNotFoundError(f"PE not found: {pe_path}")

    manifest = load_manifest(root, pe_path)
    program = find_program(manifest, export_name)
    pe_kind = str(manifest.get("pe_kind", "dll"))

    if pe_kind == "exe":
        argv = [str(pe_path), *run_args]
        result = subprocess.run(argv, check=False)
        return int(result.returncode)

    symbol = str(program["export_symbol"])
    dll = ctypes.CDLL(str(pe_path))
    try:
        func = getattr(dll, symbol)
    except AttributeError as exc:
        raise ValueError(f"missing export {symbol} in {pe_path.name}") from exc

    return_type = str(program.get("return_type", "void"))
    param_count = int(program.get("param_count", 0))

    if param_count != len(run_args):
        raise ValueError(
            f"{export_name} expects {param_count} integer argument(s), got {len(run_args)}"
        )

    if param_count:
        func.argtypes = [ctypes.c_int] * param_count
        int_args = [int(value) for value in run_args]
    else:
        int_args = []

    if return_type == "int":
        func.restype = ctypes.c_int
        print(func(*int_args))
        return 0

    if return_type == "const char*":
        func.restype = ctypes.c_char_p
        result = func()
        if result is not None:
            print(result.decode())
        return 0

    func.restype = None
    func(*int_args)
    return 0
