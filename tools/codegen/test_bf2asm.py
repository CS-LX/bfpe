#!/usr/bin/env python3
"""Unit tests for bf2asm multi-program loading."""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

_CODEGEN = Path(__file__).resolve().parent
if str(_CODEGEN) not in sys.path:
    sys.path.insert(0, str(_CODEGEN))

from bf2asm import load_program, load_programs, validate_export_uniqueness  # noqa: E402


class ExportUniquenessTests(unittest.TestCase):
    def test_duplicate_export_name_rejected(self) -> None:
        root = Path(__file__).resolve().parents[2]
        add = load_program(root / "examples" / "add.bf")
        with self.assertRaisesRegex(ValueError, "duplicate export name"):
            validate_export_uniqueness([add, add])

    def test_multi_program_load(self) -> None:
        root = Path(__file__).resolve().parents[2]
        programs = load_programs(
            [root / "examples" / "add.bf", root / "examples" / "hello_world.bf"]
        )
        names = {program["export_symbol"] for program in programs}
        self.assertEqual(names, {"BF_Add", "BF_HelloWorld"})


if __name__ == "__main__":
    unittest.main()
