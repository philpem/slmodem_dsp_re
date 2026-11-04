#!/usr/bin/env python3
"""Assembler-built controls for the test-only table memory binding apparatus."""
import hashlib
import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location('test_table_binding_apparatus', ROOT / 'tools/test_table_binding.py')
binding = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(binding)
CONTEXT_SPEC = importlib.util.spec_from_file_location('testtable_context_apparatus', ROOT / 'tools/testtable_context.py')
context = importlib.util.module_from_spec(CONTEXT_SPEC)
with mock.patch.dict(sys.modules, {'test_table_binding': binding}):
    CONTEXT_SPEC.loader.exec_module(context)


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


class FPPowBinding(unittest.TestCase):
    assemble = TableBinding.assemble

    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix='fp-pow-binding-')
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.n = 0

    def source(self, reads=None, values='16384,8192,2730,683,136,23',
               extent=None, reference=False, seventh=3, neighbor_padding='', extra=''):
        fn = 'ref_FP_Pow' if reference else 'FP_Pow'
        table = 'ref_Fact_FP' if reference else 'Fact_FP'
        if reads is None:
            reads = ['movswl {table}(%eax,%eax,1),%edx',
                     'movswl {table}(,%ebx,2),%ecx']
        text = '.text\n.globl %s\n.type %s,@function\n%s:\n' % (fn, fn, fn)
        text += '\n'.join(r.format(table=table) for r in reads)
        text += '\nret\n.size %s,.-%s\n' % (fn, fn)
        text += '.section .rodata\n.globl %s\n.type %s,@object\n%s: .short %s\n' % (table, table, table, values)
        text += '.size %s,%s\n' % (table, extent if extent is not None else '.-'+table)
        if reference:
            text += neighbor_padding
            text += '.globl ref_IIRFilterScales\n.type ref_IIRFilterScales,@object\n'
            text += 'ref_IIRFilterScales: .short %d,3,3,0\n.size ref_IIRFilterScales,.-ref_IIRFilterScales\n' % seventh
        return self.assemble(text + extra)

    def bind(self, source, **kwargs):
        options = dict(allowed_addends=(0,), profile='fp-pow')
        options.update(kwargs)
        return binding.bind_table_read(source, 'FP_Pow', 'Fact_FP', 'ref_Fact_FP',
                                       self.root/'bound.o', **options)

    def rejects(self, source, **kwargs):
        original = source.read_bytes()
        output = self.root/'bound.o'
        with self.assertRaises(binding.Refused):
            self.bind(source, **kwargs)
        self.assertEqual(source.read_bytes(), original)
        self.assertFalse(output.exists())

    def test_two_signed_reads_and_unrelated_consumers_preserved(self):
        extra = '.text\n.globl other\n.type other,@function\nother: movswl Fact_FP(%eax,%eax,1),%edx\nret\n.size other,.-other\n'
        source = self.source(extra=extra)
        before = source.read_bytes()
        record = self.bind(source)
        self.assertEqual(len(record['reads']), 2)
        self.assertEqual([r['new_addend'] for r in record['reads']], [0, 0])
        self.assertEqual(record['old_table_preserved_hex'], binding.FP_POW_COEFFICIENTS.hex())
        self.assertEqual(source.read_bytes(), before)
        _, elf, _, _ = binding._read(self.root/'bound.o')
        _, oldelf, _, _ = binding._read(source)
        oldrel = binding._snapshot(oldelf)[1]['.rel.text']
        newrel = binding._snapshot(elf)[1]['.rel.text']
        self.assertEqual(oldrel[2], newrel[2])

    def test_width_signedness_index_and_site_controls(self):
        good = 'movswl {table}(%eax,%eax,1),%edx'
        bad = ['movzwl {table}(%eax,%eax,1),%edx',
               'movsbl {table}(%eax,%eax,1),%edx',
               'movl {table}(,%eax,2),%edx',
               'movswl {table}(,%eax,4),%edx',
               'movswl {table}(%eax,%ebx,1),%edx',
               'movswl {table}+2(%eax,%eax,1),%edx',
               'movswl {table}-2(%eax,%eax,1),%edx']
        for read in bad:
            for site in (0, 1):
                with self.subTest(read=read, site=site):
                    reads = [good, good]
                    reads[site] = read
                    self.rejects(self.source(reads))
        for count in (0, 1, 3):
            with self.subTest(count=count):
                self.rejects(self.source([good]*count))

    def test_table_extent_values_and_explicit_profile(self):
        self.rejects(self.source(extent=14))
        self.rejects(self.source(values='16385,8192,2730,683,136,23'))
        self.rejects(self.source(), allowed_addends=(-2,))
        self.rejects(self.source(), profile='d302', allowed_addends=(-2,))
        self.rejects(self.source(), profile='unknown')

    def test_ambiguous_owner_and_defined_destination(self):
        self.rejects(self.source(extra='.globl alias\n.type alias,@function\n.set alias,FP_Pow\n.size alias,17\n'))
        self.rejects(self.source(extra='.data\n.globl ref_Fact_FP\nref_Fact_FP: .long 0\n'))

    def test_atomic_failure_and_cached_path_replacement(self):
        source = self.source()
        self.bind(source)
        output = self.root/'bound.o'
        before, stamp = output.read_bytes(), output.stat().st_mtime_ns
        # Same pathname, changed opcode: decoder cache must not admit old bytes.
        bad = self.source(['movzwl {table}(%eax,%eax,1),%edx',
                           'movswl {table}(,%ebx,2),%ecx'])
        source.write_bytes(bad.read_bytes())
        with self.assertRaises(binding.Refused):
            self.bind(source)
        self.assertEqual((output.read_bytes(), output.stat().st_mtime_ns), (before, stamp))

    def test_postcondition_failure_never_publishes(self):
        source = self.source()
        self.bind(source)
        output = self.root/'bound.o'
        before, stamp = output.read_bytes(), output.stat().st_mtime_ns
        original = binding._snapshot
        calls = []
        def disagree(elf):
            symbols, relocs = original(elf)
            calls.append(1)
            if len(calls) == 2:
                symbols = symbols[:-1]
            return symbols, relocs
        with mock.patch.object(binding, '_snapshot', side_effect=disagree):
            with self.assertRaises(binding.Refused):
                self.bind(source)
        self.assertEqual((output.read_bytes(), output.stat().st_mtime_ns), (before, stamp))

    def test_same_input_output_refused(self):
        source = self.source()
        with self.assertRaises(binding.Refused):
            binding.bind_table_read(source, 'FP_Pow', 'Fact_FP', 'ref_Fact_FP', source,
                                   allowed_addends=(0,), profile='fp-pow')

    def test_reference_cannot_be_overwritten(self):
        source, reference = self.source(), self.source(reference=True)
        original = reference.read_bytes()
        with self.assertRaises(binding.Refused):
            context.fp_pow(source, reference, reference)
        self.assertEqual(reference.read_bytes(), original)

    def test_malformed_elf_and_table_type_refused(self):
        broken = self.root/'broken.o'
        broken.write_bytes(b'not an ELF object')
        self.rejects(broken)
        self.rejects(self.source(extra='.type Fact_FP,@function\n'))

    def test_context_historical_and_synthetic_stable_publication(self):
        source, reference = self.source(), self.source(reference=True)
        output = self.root/'bound.o'
        record = context.fp_pow(source, reference, output)
        self.assertEqual(record['historical_reference']['seventh_word'], 3)
        before, stamp = output.read_bytes(), output.stat().st_mtime_ns
        context.fp_pow(source, reference, output)
        self.assertEqual((output.read_bytes(), output.stat().st_mtime_ns), (before, stamp))
        directory = self.root/'seventh'
        records = context.fp_pow_seventh(output, reference, directory)
        self.assertEqual([r['new_symbol'] for r in records], ['fp_pow_probe_backing']*2)
        self.assertEqual([len(r['reads']) for r in records], [2, 2])
        snapshots = [(p.read_bytes(), p.stat().st_mtime_ns) for p in (directory/'source.o', directory/'ref.o')]
        context.fp_pow_seventh(output, reference, directory)
        self.assertEqual(snapshots, [(p.read_bytes(), p.stat().st_mtime_ns) for p in (directory/'source.o', directory/'ref.o')])

    def test_historical_neighbor_value_offset_and_type_rejected(self):
        source = self.source()
        bad = [self.source(reference=True, seventh=4),
               self.source(reference=True, neighbor_padding='.short 0\n'),
               self.source(reference=True, extra='.type ref_IIRFilterScales,@function\n')]
        for reference in bad:
            with self.subTest(reference=reference):
                with self.assertRaises(binding.Refused):
                    context.fp_pow(source, reference, self.root/'bound.o')
                self.assertFalse((self.root/'bound.o').exists())

    def test_synthetic_failure_preserves_both_outputs(self):
        source, reference = self.source(), self.source(reference=True)
        bound = self.root/'bound.o'
        context.fp_pow(source, reference, bound)
        directory = self.root/'seventh'
        context.fp_pow_seventh(bound, reference, directory)
        paths = [directory/'source.o', directory/'ref.o']
        before = [(p.read_bytes(), p.stat().st_mtime_ns) for p in paths]
        bad = self.source(reference=True, seventh=4)
        with self.assertRaises(binding.Refused):
            context.fp_pow_seventh(bound, bad, directory)
        self.assertEqual(before, [(p.read_bytes(), p.stat().st_mtime_ns) for p in paths])


if __name__ == '__main__':
    unittest.main(verbosity=2)
