import shutil
import tempfile
import unittest
from pathlib import Path

from check_device_core import check


class DeviceDiscipline(unittest.TestCase):
    def inspect(self, source):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "test.hpp").write_text("#define SPIRE_FN\n" + source)
            return check(root, shutil.which("clang++"))

    def test_valid_fixed_state(self):
        self.assertEqual([], self.inspect(
            "#include <cstdint>\nconstexpr int capacity = 8;\n"
            "struct Game { int values[capacity]; SPIRE_FN int value() const { return values[0]; } };\n"
            "SPIRE_FN void step(Game& game) { game.values[0] += 1; }\n"
        ))

    def test_forbidden_header(self):
        self.assertTrue(self.inspect("#include <vector>\n"))

    def test_macro_header_is_rejected(self):
        self.assertTrue(self.inspect("#define HEADER <vector>\n#include HEADER\n"))

    def test_heap_and_exceptions(self):
        for source in ("SPIRE_FN int* f() { return new int; }", "SPIRE_FN void f() { throw 1; }"):
            with self.subTest(source=source):
                self.assertTrue(self.inspect(source))

    def test_unannotated_function(self):
        self.assertTrue(self.inspect("int f() { return 1; }"))

    def test_observation_extra_root_is_checked(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "core").mkdir()
            (root / "obs").mkdir()
            (root / "core/platform.hpp").write_text("#define SPIRE_FN\n")
            source = root / "obs/encode.hpp"
            source.write_text('#include "core/platform.hpp"\nint encode() { return 1; }\n')
            errors = check(root / "core", shutil.which("clang++"), extra_roots=[root / "obs"])
            self.assertTrue(any("encode lacks SPIRE_FN" in error for error in errors))
            source.write_text('#include "core/platform.hpp"\n#include <vector>\n')
            errors = check(root / "core", shutil.which("clang++"), extra_roots=[root / "obs"])
            self.assertTrue(any("forbidden header vector" in error for error in errors))

    def test_mutable_global(self):
        self.assertTrue(self.inspect("int counter = 0;"))

    def test_static_local(self):
        self.assertTrue(self.inspect("SPIRE_FN int f() { static int x = 0; return ++x; }"))

    def test_function_pointer(self):
        self.assertTrue(self.inspect("struct Game { int (*f)(int); };"))

    def test_recursion(self):
        self.assertTrue(self.inspect("SPIRE_FN int f(int n) { return n ? f(n - 1) : 0; }"))

    def test_mutual_recursion(self):
        self.assertTrue(self.inspect("SPIRE_FN int g(int); SPIRE_FN int f(int n) { return g(n); } SPIRE_FN int g(int n) { return f(n); }"))


if __name__ == "__main__":
    unittest.main()
