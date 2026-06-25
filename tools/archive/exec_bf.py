#!/usr/bin/env python3
"""In-memory Brainfuck execution for bfpe exec (BFC-0 semantics)."""

from __future__ import annotations

import sys
from pathlib import Path

_CODEGEN = Path(__file__).resolve().parent / "codegen"
if str(_CODEGEN) not in sys.path:
    sys.path.insert(0, str(_CODEGEN))

from bf2asm import strip_bf
from parse_sig import parse_file

TAPE_SIZE = 65536
STEP_LIMIT = 10_000_000
BF_CHARS = set("+-<>[],.")


class BfError(Exception):
    pass


def build_jump_table(code: str) -> dict[int, int]:
    stack: list[int] = []
    pairs: dict[int, int] = {}
    for index, char in enumerate(code):
        if char == "[":
            stack.append(index)
        elif char == "]":
            if not stack:
                raise BfError("unmatched ']'")
            open_index = stack.pop()
            pairs[open_index] = index
            pairs[index] = open_index
    if stack:
        raise BfError("unmatched '['")
    return pairs


def trim_output(text: str) -> str:
    while text and text[-1] in "\r\n":
        text = text[:-1]
    return text


def run_program(
    code: str,
    tape: list[int],
    *,
    io_mode: str,
    stdin_data: list[int] | None = None,
) -> str:
    pointer = 0
    ip = 0
    steps = 0
    jumps = build_jump_table(code)
    output: list[str] = []
    stdin_index = 0
    stdin_data = stdin_data or []

    while ip < len(code):
        op = code[ip]
        if op not in BF_CHARS:
            ip += 1
            continue

        if op == "+":
            tape[pointer] = (tape[pointer] + 1) & 0xFF
            ip += 1
        elif op == "-":
            tape[pointer] = (tape[pointer] - 1) & 0xFF
            ip += 1
        elif op == ">":
            pointer = (pointer + 1) % TAPE_SIZE
            ip += 1
        elif op == "<":
            pointer = (pointer + TAPE_SIZE - 1) % TAPE_SIZE
            ip += 1
        elif op == ".":
            byte = tape[pointer] & 0xFF
            if io_mode in ("buffer", "stdio"):
                output.append(chr(byte))
            if io_mode == "stdio":
                sys.stdout.write(chr(byte))
                sys.stdout.flush()
            ip += 1
        elif op == ",":
            if io_mode == "stdio" and not stdin_data:
                ch = sys.stdin.read(1)
                tape[pointer] = 0 if not ch else ord(ch) & 0xFF
            elif stdin_index < len(stdin_data):
                tape[pointer] = stdin_data[stdin_index] & 0xFF
                stdin_index += 1
            else:
                tape[pointer] = 0
            ip += 1
        elif op == "[":
            if tape[pointer] == 0:
                ip = jumps[ip] + 1
            else:
                ip += 1
        elif op == "]":
            if tape[pointer] != 0:
                ip = jumps[ip] + 1
            else:
                ip += 1

        steps += 1
        if steps > STEP_LIMIT:
            raise BfError("step limit exceeded")

    return "".join(output)


def exec_bf(bf_path: Path, run_args: list[str]) -> int:
    bf_path = bf_path.resolve()
    sig = parse_file(bf_path)
    code = strip_bf(bf_path.read_text(encoding="utf-8"))
    tape = [0] * TAPE_SIZE

    if len(run_args) != len(sig.params):
        expected = len(sig.params)
        raise ValueError(
            f"{sig.export_name} expects {expected} integer argument(s), got {len(run_args)}"
        )

    for index, arg_text in enumerate(run_args):
        tape[index] = int(arg_text) & 0xFF

    output = run_program(code, tape, io_mode=sig.io_mode)

    if sig.return_type == "int":
        print(tape[0] & 0xFF)
    elif sig.return_type == "const char*":
        trimmed = trim_output(output)
        if trimmed:
            print(trimmed)
    elif sig.return_type == "void":
        if sig.io_mode == "stdio":
            sys.stdout.flush()

    return 0


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("usage: exec_bf.py <file.bf> [args...]", file=sys.stderr)
        raise SystemExit(1)
    try:
        raise SystemExit(exec_bf(Path(sys.argv[1]), sys.argv[2:]))
    except (BfError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(1)
