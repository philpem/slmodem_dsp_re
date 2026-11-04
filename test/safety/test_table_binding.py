#!/usr/bin/env python3
"""Assembler-built controls for the test-only table memory binding apparatus."""
import hashlib
import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location('test_table_binding_apparatus', ROOT / 'tools/test_table_binding.py')
binding = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(binding)


class TableBinding(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix='table-binding-')
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.n = 0
        self.context = self.assemble('.data\n.globl table\n.type table,@object\ntable: .short 5792,12953,17378\n.size table,.-table\n')

    def assemble(self, text):
        self.n += 1
        source = self.root / ('control%d.s' % self.n)
        source.write_text(text)
        output = source.with_suffix('.o')
        subprocess.run(['as', '--32', str(source), '-o', str(output)], check=True, capture_output=True)
        return output

    def source(self, read='movzwl table-2(%eax,%eax,1),%edx', extra='', defined=False):
        text = '.text\n.globl owner\n.type owner,@function\nowner:\n' + read + '\nret\n.size owner,.-owner\n' + extra
        if defined:
            text += '.data\n.globl table\n.type table,@object\ntable: .short 5792,12953,17378\n.size table,.-table\n'
        return self.assemble(text)

    def bind(self, source, **kwargs):
        return binding.bind_table_read(source, 'owner', 'table', 'external', self.root / 'bound.o', **kwargs)

    def rejects(self, source, **kwargs):
        original = source.read_bytes()
        with self.assertRaises(binding.Refused):
            self.bind(source, **kwargs)
        self.assertEqual(source.read_bytes(), original)
        self.assertFalse((self.root / 'bound.o').exists())

    def test_actual_movzwl_with_explicit_definition(self):
        source = self.source()
        original = source.read_bytes()
        record = self.bind(source, table_object=self.context)
        self.assertEqual(record['read_raw_before'], '0fb79400feffffff')
        self.assertEqual(source.read_bytes(), original)
        self.assertEqual(record['input_sha256'], hashlib.sha256(original).hexdigest())
        self.assertEqual(record['old_table_preserved_hex'], 'a0169932e243')

    def test_defined_six_byte_table(self):
        self.bind(self.source(defined=True))

    def test_movswl_no_base_scale_two(self):
        self.bind(self.source('movswl table-2(,%eax,2),%edx', defined=True))

    def test_existing_undefined_symbol_and_byte_stable_publish(self):
        source = self.source(extra='.data\n.long external\n', defined=True)
        record = self.bind(source)
        self.assertIsNone(record['objcopy_command'])
        output = self.root / 'bound.o'
        before = output.stat().st_mtime_ns
        self.bind(source)
        self.assertEqual(before, output.stat().st_mtime_ns)

    def test_wrong_allowed_addend(self):
        self.rejects(self.source(defined=True), allowed_addends=(0,))

    def test_wrong_width(self):
        self.rejects(self.source('movzbl table-2(%eax,%eax,1),%edx', defined=True))

    def test_wrong_target(self):
        self.rejects(self.source('movzwl table(%eax,%eax,1),%edx', defined=True))

    def test_multiple_matching_reads(self):
        self.rejects(self.source('movzwl table-2(%eax,%eax,1),%edx\nmovzwl table-2(%ebx,%ebx,1),%ecx', defined=True))

    def test_ambiguous_function_owner(self):
        extra = '.globl alias\n.type alias,@function\n.set alias,owner\n.size alias,9\n'
        self.rejects(self.source(extra=extra, defined=True))

    def test_missing_table_context(self):
        self.rejects(self.source())

    def test_wrong_table_context(self):
        wrong = self.assemble('.data\n.globl other\nother: .short 5792,12953,17378\n')
        self.rejects(self.source(), table_object=wrong)

    def test_wrong_table_bytes(self):
        wrong = self.assemble('.data\n.globl table\n.type table,@object\ntable: .short 5793,12953,17378\n.size table,.-table\n')
        self.rejects(self.source(), table_object=wrong)


if __name__ == '__main__':
    unittest.main(verbosity=2)
