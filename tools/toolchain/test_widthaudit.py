#!/usr/bin/env python3
"""Exercise width-audit reporting with known ELF32 instruction differences."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
TOOL = ROOT / 'tools/toolchain/widthaudit.py'


class WidthAuditReportingTests(unittest.TestCase):
    def setUp(self):
        scratch = ROOT / 'scratch'
        scratch.mkdir(exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(prefix='widthaudit-test-', dir=scratch)
        self.addCleanup(self.temp.cleanup)
        self.folder = Path(self.temp.name)
        self.ours = self.folder / 'objects'
        self.ours.mkdir()

    def assemble(self, path, instruction, symbol='width_fixture'):
        source = path.with_suffix('.s')
        source.write_text('.text\n.globl ' + symbol + '\n.type ' + symbol +
                          ', @function\n' + symbol + ':\n' + instruction +
                          '\nret\n.size ' + symbol + ', .-' + symbol + '\n')
        subprocess.run(['as', '--32', '-o', str(path), str(source)], check=True,
                       capture_output=True, text=True)

    def audit(self, *options):
        return subprocess.run([sys.executable, str(TOOL), *options], cwd=ROOT,
                              env=dict(os.environ, BLOB=str(self.folder / 'reference.o'),
                                       TC_OUT=str(self.ours)), capture_output=True,
                              text=True)

    def test_default_reports_known_extension_conflict(self):
        self.assemble(self.folder / 'reference.o', 'movswl 16(%eax), %edx')
        self.assemble(self.ours / 'candidate.o', 'movzwl 16(%eax), %edx')
        result = self.audit()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('shared symbols: 1, with a width/extension delta: 1', result.stdout)
        self.assertIn('width_fixture', result.stdout)
        self.assertRegex(result.stdout, r'load\s+disp 0x10\s+blob s16\s+ours z16')

    def test_strong_mode_reports_the_same_known_conflict(self):
        self.assemble(self.folder / 'reference.o', 'movswl 16(%eax), %edx')
        self.assemble(self.ours / 'candidate.o', 'movzwl 16(%eax), %edx')
        result = self.audit('--strong')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('STRONG single-kind mismatches', result.stdout)
        self.assertIn('1 symbol(s)', result.stdout)
        self.assertIn('width_fixture', result.stdout)

    def test_identical_accesses_report_a_clean_nonempty_denominator(self):
        self.assemble(self.folder / 'reference.o', 'movswl 16(%eax), %edx')
        self.assemble(self.ours / 'candidate.o', 'movswl 16(%eax), %edx')
        result = self.audit()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('shared symbols: 1, with a width/extension delta: 0', result.stdout)

    def test_no_shared_symbols_is_refused(self):
        self.assemble(self.folder / 'reference.o', 'movswl 16(%eax), %edx', 'reference_only')
        self.assemble(self.ours / 'candidate.o', 'movswl 16(%eax), %edx', 'candidate_only')
        result = self.audit()
        self.assertEqual(result.returncode, 2, result.stderr)
        self.assertIn('no shared symbols -- refusing', result.stderr)


if __name__ == '__main__':
    unittest.main()
