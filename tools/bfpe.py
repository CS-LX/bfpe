#!/usr/bin/env python3
"""BFPE command-line tool — Phase 0: build single .bf to DLL."""

from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RUNTIME = ROOT / "runtime"
CODEGEN = ROOT / "tools" / "codegen" / "bf2asm.py"
VERIFY_SCRIPT = ROOT / "tools" / "verify_pe.ps1"

RUNTIME_SOURCES = [
    RUNTIME / "vm" / "bf_vm.c",
    RUNTIME / "bf_io.c",
    RUNTIME / "bf_stub.c",
    RUNTIME / "dllmain.c",
]

RUNTIME_INCLUDES = [
    RUNTIME,
    RUNTIME / "vm",
]

BASE_RUNTIME_EXPORTS = ["BF_GetLastOutput", "BF_SetOutputCallback"]


def eprint(*args: object) -> None:
    print(*args, file=sys.stderr)


def find_vs_install() -> Path | None:
    program_files_x86 = os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")
    vswhere = Path(program_files_x86) / "Microsoft Visual Studio" / "Installer" / "vswhere.exe"
    if not vswhere.is_file():
        return None

    result = subprocess.run(
        [
            str(vswhere),
            "-latest",
            "-requires",
            "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
            "-property",
            "installationPath",
        ],
        capture_output=True,
        text=True,
        check=False,
    )
    install_path = result.stdout.strip()
    if result.returncode != 0 or not install_path:
        return None
    return Path(install_path)


def find_msvc_bin(install: Path) -> Path:
    tools_root = install / "VC" / "Tools" / "MSVC"
    versions = sorted(tools_root.glob("*"), reverse=True)
    for version_dir in versions:
        bin_dir = version_dir / "bin" / "Hostx64" / "x64"
        if (bin_dir / "ml64.exe").is_file():
            return bin_dir
    raise RuntimeError(f"ml64.exe not found under {tools_root}")


def msvc_env() -> dict[str, str]:
    install = find_vs_install()
    if install is None:
        raise RuntimeError(
            "Visual Studio 2022 with C++ tools not found. Install VS or run from Developer PowerShell."
        )

    vcvars = install / "VC" / "Auxiliary" / "Build" / "vcvars64.bat"
    if not vcvars.is_file():
        raise RuntimeError(f"vcvars64.bat not found: {vcvars}")

    result = subprocess.run(
        f'call "{vcvars}" >nul && set',
        capture_output=True,
        text=True,
        shell=True,
        check=False,
    )
    if result.returncode != 0:
        raise RuntimeError("Failed to initialize MSVC environment via vcvars64.bat")

    env = os.environ.copy()
    for line in result.stdout.splitlines():
        if "=" in line:
            key, value = line.split("=", 1)
            env[key] = value

    msvc_bin = find_msvc_bin(install)
    env["PATH"] = str(msvc_bin) + os.pathsep + env.get("PATH", "")
    return env


def resolve_tool(name: str, env: dict[str, str]) -> str:
    path = env.get("PATH", "")
    resolved = shutil.which(name, path=path)
    if resolved:
        return resolved
    raise RuntimeError(f"MSVC tool not found on PATH after vcvars64: {name}")


def run_cmd(
    args: list[str],
    *,
    env: dict[str, str] | None = None,
    cwd: Path | None = None,
) -> None:
    display = " ".join(f'"{arg}"' if " " in arg else arg for arg in args)
    print(f"> {display}")
    result = subprocess.run(args, env=env, cwd=cwd, check=False)
    if result.returncode != 0:
        raise RuntimeError(f"command failed ({result.returncode}): {display}")


def write_def(path: Path, library_name: str, exports: list[str]) -> None:
    lines = [f"LIBRARY {library_name}", "EXPORTS"]
    lines.extend(f"    {name}" for name in exports)
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def load_manifest_programs(manifest_path: Path) -> list[dict[str, object]]:
    data = json.loads(manifest_path.read_text(encoding="utf-8"))
    return list(data.get("programs", []))


def cmd_build(args: argparse.Namespace) -> int:
    if len(args.inputs) != 1:
        eprint("error: Phase 0 supports exactly one .bf input")
        return 1

    bf_path = args.inputs[0].resolve()
    output = args.output.resolve()

    if not bf_path.is_file():
        eprint(f"error: {bf_path} not found")
        return 1
    if output.suffix.lower() != ".dll":
        eprint("error: Phase 0 output must be a .dll file")
        return 1

    build_dir = ROOT / ".bfpe-build" / output.stem
    gen_dir = build_dir / "gen"
    obj_dir = build_dir / "obj"
    gen_dir.mkdir(parents=True, exist_ok=True)
    obj_dir.mkdir(parents=True, exist_ok=True)
    output.parent.mkdir(parents=True, exist_ok=True)

    gen_asm = gen_dir / "bf_programs.asm"
    gen_header = gen_dir / "bf_exports.gen.h"
    gen_source = gen_dir / "bf_exports.gen.c"
    manifest_path = build_dir / "manifest.json"
    def_path = build_dir / f"{output.stem}.def"

    python = sys.executable
    run_cmd(
        [
            python,
            str(CODEGEN),
            str(bf_path),
            "-o",
            str(gen_asm),
            "--header",
            str(gen_header),
            "--source",
            str(gen_source),
            "--manifest",
            str(manifest_path),
            "--dll-path",
            str(output),
        ]
    )

    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    generated_exports = [
        str(program["export_symbol"])
        for program in manifest["programs"]
        if program.get("export_symbol")
    ]
    if not generated_exports:
        eprint("error: no exports generated from .bf input")
        return 1

    write_def(def_path, output.stem, generated_exports + BASE_RUNTIME_EXPORTS)

    env = msvc_env()
    ml64 = resolve_tool("ml64.exe", env)
    cl = resolve_tool("cl.exe", env)
    link = resolve_tool("link.exe", env)

    asm_obj = obj_dir / "bf_programs.obj"
    run_cmd([ml64, "/c", "/Fo", str(asm_obj), str(gen_asm)], env=env)

    compile_flags = [
        cl,
        "/nologo",
        "/O2",
        "/W4",
        "/c",
        "/DBFDLL_EXPORTS",
    ]
    for include in RUNTIME_INCLUDES + [gen_dir]:
        compile_flags.extend(["/I", str(include)])

    c_objects: list[Path] = []
    for source in [*RUNTIME_SOURCES, gen_source]:
        obj_path = obj_dir / f"{source.stem}.obj"
        run_cmd([*compile_flags, f"/Fo{obj_path}", str(source)], env=env)
        c_objects.append(obj_path)

    include_flags = []
    for program in manifest["programs"]:
        symbol = program["program_symbol"]
        include_flags.extend(["/INCLUDE:" + symbol])

    link_args = [
        link,
        "/nologo",
        "/DLL",
        f"/OUT:{output}",
        f"/DEF:{def_path}",
        "/SUBSYSTEM:WINDOWS",
        "/MERGE:bf_text=.text",
        *include_flags,
        *[str(obj) for obj in [asm_obj, *c_objects]],
    ]
    run_cmd(link_args, env=env)

    manifest["pe_path"] = str(output)
    manifest_path.write_text(json.dumps(manifest, indent=2), encoding="utf-8")

    run_cmd(
        [
            "powershell",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            str(VERIFY_SCRIPT),
            "-ManifestPath",
            str(manifest_path),
        ]
    )

    print(f"Built and verified: {output}")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(prog="bfpe", description="Brainfuck-in-PE toolchain")
    subparsers = parser.add_subparsers(dest="command")

    build_parser = subparsers.add_parser("build", help="Build .bf into a PE DLL")
    build_parser.add_argument("inputs", nargs="+", type=Path, help=".bf source file(s)")
    build_parser.add_argument("-o", "--output", type=Path, required=True, help="Output .dll path")

    args = parser.parse_args()
    if args.command == "build":
        return cmd_build(args)

    parser.print_help()
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
