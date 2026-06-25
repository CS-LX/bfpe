#!/usr/bin/env python3
"""Unit tests for parse_sig."""

from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

from parse_sig import Signature, parse_file, parse_signature_line


class ParseSignatureLineTests(unittest.TestCase):
    def test_int_function(self) -> None:
        ret, name, params = parse_signature_line("int add(int a, int b)")
        self.assertEqual(ret, "int")
        self.assertEqual(name, "add")
        self.assertEqual([(p.type_name, p.name) for p in params], [("int", "a"), ("int", "b")])

    def test_const_char_star(self) -> None:
        ret, name, params = parse_signature_line("const char* hello(void)")
        self.assertEqual(ret, "const char*")
        self.assertEqual(name, "hello")
        self.assertEqual(params, [])

    def test_void(self) -> None:
        ret, name, params = parse_signature_line("void hello(void)")
        self.assertEqual(ret, "void")
        self.assertEqual(name, "hello")
        self.assertEqual(params, [])


class ParseFileTests(unittest.TestCase):
    def write_bf(self, text: str) -> Path:
        tmp = tempfile.NamedTemporaryFile("w", suffix=".bf", delete=False, encoding="utf-8")
        tmp.write(text)
        tmp.close()
        self.addCleanup(lambda: Path(tmp.name).unlink(missing_ok=True))
        return Path(tmp.name)

    def test_bfpe_add(self) -> None:
        path = self.write_bf(
            "; bfpe: export=Add\n; bfpe: int add(int a, int b)\n>[-<+>]\n"
        )
        sig = parse_file(path)
        self.assertEqual(sig.export_symbol, "BF_Add")
        self.assertEqual(sig.return_type, "int")
        self.assertEqual(sig.io_mode, "none")
        self.assertEqual(len(sig.params), 2)

    def test_bfpe_stdio(self) -> None:
        path = self.write_bf(
            "; bfpe: export=Hello\n; bfpe: void hello(void)\n; bfpe: io=stdio\n.\n"
        )
        sig = parse_file(path)
        self.assertEqual(sig.export_symbol, "BF_Hello")
        self.assertEqual(sig.io_mode, "stdio")

    def test_legacy_bfdll(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "add.bf"
            path.write_text("; bfdll: export=output\n>[-<+>]\n", encoding="utf-8")
            sig = parse_file(path)
            self.assertEqual(sig.export_symbol, "BF_Add")
            self.assertEqual(sig.return_type, "const char*")
            self.assertEqual(sig.io_mode, "buffer")


if __name__ == "__main__":
    unittest.main()
