#!/usr/bin/env python3
"""Parse ; bfpe: / ; bfdll: signature directives from .bf sources."""

from __future__ import annotations

import re
from dataclasses import dataclass, field
from pathlib import Path

BFPE_PREFIX = "bfpe:"
BFDLL_EXPORT_OUTPUT = "bfdll: export=output"

SIGNATURE_RE = re.compile(
    r"^(?P<ret>void|int|const\s+char\s*\*)\s+"
    r"(?P<name>[A-Za-z_]\w*)\s*"
    r"\((?P<params>[^)]*)\)\s*$"
)
PARAM_RE = re.compile(r"^int\s+(?P<name>[A-Za-z_]\w*)\s*$")
IO_RE = re.compile(r"^io\s*=\s*(?P<mode>buffer|stdio|none)\s*$", re.IGNORECASE)
EXPORT_RE = re.compile(r"^export\s*=\s*(?P<name>[A-Za-z_]\w*)\s*$")


@dataclass
class Param:
    type_name: str
    name: str


@dataclass
class Signature:
    export_name: str
    export_symbol: str
    return_type: str
    c_name: str
    params: list[Param] = field(default_factory=list)
    io_mode: str = "buffer"
    program_symbol: str = ""
    source_name: str = ""

    @property
    def io_mode_c(self) -> str:
        return {
            "buffer": "BF_IO_MODE_BUFFER",
            "stdio": "BF_IO_MODE_STDIO",
            "none": "BF_IO_MODE_NONE",
        }[self.io_mode]

    def c_declaration(self) -> str:
        if self.return_type == "const char*":
            ret = "const char*"
        else:
            ret = self.return_type
        params = ", ".join(f"int {param.name}" for param in self.params)
        return f"{ret} __cdecl {self.export_symbol}({params})"


def program_symbol_from_path(path: Path) -> str:
    stem = path.stem.replace("-", "_").title().replace("_", "")
    return "BF_Prog_" + stem


def export_symbol_from_name(export_name: str) -> str:
    return "BF_" + export_name


def default_io_mode(return_type: str) -> str:
    if return_type == "const char*":
        return "buffer"
    if return_type == "void":
        return "stdio"
    return "none"


def parse_params(params_text: str) -> list[Param]:
    params_text = params_text.strip()
    if not params_text or params_text.lower() == "void":
        return []

    params: list[Param] = []
    for chunk in params_text.split(","):
        chunk = chunk.strip()
        if not chunk:
            continue
        match = PARAM_RE.match(chunk)
        if not match:
            raise ValueError(f"unsupported parameter: {chunk!r} (MVP supports int only)")
        params.append(Param(type_name="int", name=match.group("name")))
    return params


def parse_signature_line(line: str) -> tuple[str, str, list[Param]]:
    match = SIGNATURE_RE.match(line.strip())
    if not match:
        raise ValueError(f"invalid signature line: {line!r}")

    raw_ret = match.group("ret")
    if "char" in raw_ret:
        return_type = "const char*"
    elif raw_ret == "void":
        return_type = "void"
    else:
        return_type = "int"

    c_name = match.group("name")
    params = parse_params(match.group("params"))
    return return_type, c_name, params


def parse_directives(source: str) -> tuple[str | None, str | None, list[Param], str | None, bool]:
    export_name: str | None = None
    signature_line: str | None = None
    params: list[Param] = []
    io_mode: str | None = None
    legacy_output = False

    for raw_line in source.splitlines():
        if ";" not in raw_line:
            continue
        comment = raw_line.split(";", 1)[1].strip()
        if not comment:
            continue

        if BFDLL_EXPORT_OUTPUT in comment:
            legacy_output = True
            continue

        if not comment.lower().startswith(BFPE_PREFIX):
            continue

        payload = comment[len(BFPE_PREFIX) :].strip()
        if not payload:
            continue

        export_match = EXPORT_RE.match(payload)
        if export_match:
            export_name = export_match.group("name")
            continue

        io_match = IO_RE.match(payload)
        if io_match:
            io_mode = io_match.group("mode").lower()
            continue

        if payload == "entry":
            continue

        try:
            return_type, c_name, parsed_params = parse_signature_line(payload)
        except ValueError:
            continue

        signature_line = payload
        params = parsed_params

    return export_name, signature_line, params, io_mode, legacy_output


def parse_file(path: Path) -> Signature:
    source = path.read_text(encoding="utf-8")
    export_name, signature_line, params, io_mode, legacy_output = parse_directives(source)

    if legacy_output and export_name is None and signature_line is None:
        stem = path.stem.replace("-", "_").title().replace("_", "")
        export_name = stem
        return_type = "const char*"
        c_name = path.stem.replace("-", "_")
        io_mode = io_mode or "buffer"
    elif export_name is None or signature_line is None:
        raise ValueError(
            f"{path.name}: expected '; bfpe: export=<Name>' and a C-style signature line "
            f"(or legacy '; bfdll: export=output')"
        )
    else:
        return_type, c_name, params = parse_signature_line(signature_line)
        io_mode = io_mode or default_io_mode(return_type)

    if io_mode not in {"buffer", "stdio", "none"}:
        raise ValueError(f"{path.name}: unsupported io mode {io_mode!r}")

    return Signature(
        export_name=export_name,
        export_symbol=export_symbol_from_name(export_name),
        return_type=return_type,
        c_name=c_name,
        params=params,
        io_mode=io_mode,
        program_symbol=program_symbol_from_path(path),
        source_name=path.name,
    )
