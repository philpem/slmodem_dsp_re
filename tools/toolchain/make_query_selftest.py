#!/usr/bin/env python3
"""Prove variable queries skip dependencies while build goals still load them."""

from pathlib import Path
import os
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]


class QueryDependencies(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="make-query-")
        self.addCleanup(self.directory.cleanup)
        self.build = Path(self.directory.name)
        (self.build / "control.d").write_text(
            "$(error dependency scan control fired)\n")

    def run_make(self, *goals):
        env = dict(os.environ)
        for name in ("MAKEFLAGS", "MAKEFILES", "MFLAGS", "MAKEOVERRIDES"):
            env.pop(name, None)
        dry_run = [] if goals and all(g.startswith("print-") for g in goals) else ["-n"]
        return subprocess.run(
            ["make", "--no-print-directory", "-s", *dry_run,
             "BUILD=" + str(self.build), *goals],
            cwd=ROOT, env=env, text=True, capture_output=True)

    def test_single_query_skips_dependency_control(self):
        result = self.run_make("print-SRC")
        self.assertEqual(result.returncode, 0, result.stderr)
        expected = " ".join(sorted(str(p.relative_to(ROOT))
                                   for p in (ROOT / "src").rglob("*.c")))
        self.assertEqual(result.stdout.strip(), expected)

    def test_multiple_queries_skip_dependency_control(self):
        result = self.run_make("print-SRC", "print-CXXSRC", "print-ASRC",
                               "print-BLOB")
        self.assertEqual(result.returncode, 0, result.stderr)
        lines = result.stdout.splitlines()
        self.assertEqual(len(lines), 4)
        for line, suffix in zip(lines[:3], (".c", ".cpp", ".S")):
            expected = sorted(str(p.relative_to(ROOT))
                              for p in (ROOT / "src").rglob("*" + suffix))
            self.assertEqual(line.split(), expected)
        self.assertTrue((ROOT / lines[3]).is_file())

    def test_default_build_reads_dependency_control(self):
        result = self.run_make()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("dependency scan control fired", result.stderr)

    def test_mixed_build_and_query_reads_dependency_control(self):
        result = self.run_make("print-SRC", "all")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("dependency scan control fired", result.stderr)


if __name__ == "__main__":
    unittest.main(verbosity=2)
