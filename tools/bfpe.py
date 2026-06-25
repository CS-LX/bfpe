#!/usr/bin/env python3
"""BFPE command-line tool — build and run Brainfuck-in-PE artifacts."""

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

sys.path.insert(0, str(ROOT / "tools" / "codegen"))
from parse_sig import parse_file  # noqa: E402

sys.path.insert(0, str(ROOT / "tools"))
from run_pe import run_pe  # noqa: E402

RUNTIME_CORE = [
    RUNTIME / "vm" / "bf_vm.c",
    RUNTIME / "bf_io.c",
    RUNTIME / "bf_export_runtime.c",
    RUNTIME / "bf_stub.c",
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


def pe_kind_for_output(output: Path) -> str:
    suffix = output.suffix.lower()
    if suffix == ".dll":
        return "dll"
    if suffix == ".exe":
        return "exe"
    raise ValueError(f"unsupported output extension: {suffix} (expected .dll or .exe)")


def cmd_build(bf_path: Path, output: Path) -> int:
    if not bf_path.is_file():
        eprint(f"error: {bf_path} not found")
        return 1

    try:
        pe_kind = pe_kind_for_output(output)
    except ValueError as exc:
        eprint(f"error: {exc}")
        return 1

    bf_path = bf_path.resolve()
    output = output.resolve()

    try:
        signature = parse_file(bf_path)
    except ValueError as exc:
        eprint(f"error: {exc}")
        return 1

    if pe_kind == "exe" and not signature.is_entry:
        eprint(f"error: EXE build requires '; bfpe: entry' in {bf_path.name}")
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
    gen_exe_main = gen_dir / "exe_main.gen.c"
    manifest_path = build_dir / "manifest.json"
    def_path = build_dir / f"{output.stem}.def"

    codegen_cmd = [
        sys.executable,
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
        "--pe-path",
        str(output),
        "--pe-kind",
        pe_kind,
    ]
    if pe_kind == "exe":
        codegen_cmd.extend(["--exe-main", str(gen_exe_main)])
    run_cmd(codegen_cmd)

    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    generated_exports = [str(program["export_symbol"]) for program in manifest["programs"]]
    if not generated_exports:
        eprint("error: no exports generated from .bf input")
        return 1

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

    c_sources = [*RUNTIME_CORE, gen_source]
    if pe_kind == "dll":
        c_sources.append(RUNTIME / "dllmain.c")
    else:
        c_sources.append(gen_exe_main)

    c_objects: list[Path] = []
    for source in c_sources:
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
        f"/OUT:{output}",
        "/MERGE:bf_text=.text",
        *include_flags,
        *[str(obj) for obj in [asm_obj, *c_objects]],
    ]

    if pe_kind == "dll":
        write_def(def_path, output.stem, generated_exports + BASE_RUNTIME_EXPORTS)
        link_args[2:2] = [
            "/DLL",
            f"/DEF:{def_path}",
            "/SUBSYSTEM:WINDOWS",
        ]
    else:
        link_args.insert(2, "/SUBSYSTEM:CONSOLE")

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


def cmd_run(pe_path: Path, export_name: str, run_args: list[str]) -> int:
    try:
        return run_pe(ROOT, pe_path, export_name, run_args)
    except (FileNotFoundError, ValueError) as exc:
        eprint(f"error: {exc}")
        return 1


def try_shorthand(argv: list[str]) -> int | None:
    if not argv or argv[0] in ("build", "run", "-h", "--help"):
        return None

    if "-o" in argv:
        out_index = argv.index("-o")
        if out_index + 1 >= len(argv):
            eprint("error: shorthand build: bfpe <file.bf> -o <out.pe>")
            return 1
        bf_path = Path(argv[0])
        output = Path(argv[out_index + 1])
        return cmd_build(bf_path, output)

    if len(argv) >= 2:
        pe_path = Path(argv[-1])
        if pe_path.suffix.lower() in (".dll", ".exe") and pe_path.exists():
            bf_path = Path(argv[0])
            if bf_path.suffix.lower() != ".bf":
                return None
            try:
                signature = parse_file(bf_path)
            except ValueError as exc:
                eprint(f"error: {exc}")
                return 1
            run_args = argv[1:-1]
            return cmd_run(pe_path, signature.export_name, run_args)

    return None


def main() -> int:
    shorthand = try_shorthand(sys.argv[1:])
    if shorthand is not None:
        return shorthand

    parser = argparse.ArgumentParser(prog="bfpe", description="Brainfuck-in-PE toolchain")
    subparsers = parser.add_subparsers(dest="command")

    build_parser = subparsers.add_parser("build", help="Build .bf into a PE DLL or EXE")
    build_parser.add_argument("input", type=Path, help=".bf source file")
    build_parser.add_argument("-o", "--output", type=Path, required=True, help="Output .dll/.exe")

    run_parser = subparsers.add_parser("run", help="Run a built PE")
    run_parser.add_argument("pe", type=Path, help="Path to .dll or .exe")
    run_parser.add_argument("export", help="Export name (e.g. Add, Hello)")
    run_parser.add_argument("args", nargs="*", help="Integer arguments for exported function")

    args = parser.parse_args()
    if args.command == "build":
        return cmd_build(args.input, args.output)
    if args.command == "run":
        return cmd_run(args.pe, args.export, args.args)

    parser.print_help()
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
