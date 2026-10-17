#!/usr/bin/env python3
"""Real ELF controls for section-scoped disassembly and inline relocations."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
DIS = ROOT / 'tools/dis.py'


class SectionDisassembly(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix='dis-sections-')
        self.root = Path(self.directory.name)
        self.addCleanup(self.directory.cleanup)
        self.object = self.assemble('overlap', '''
.text
.globl ordinary
.type ordinary,@function
ordinary: call text_target
ret
.size ordinary,.-ordinary
.section .gnu.linkonce.t.first,"ax",@progbits
.globl first
.type first,@function
first: call first_target
.byte 0xde,0xe1
ret
.size first,.-first
.section .gnu.linkonce.t.second,"ax",@progbits
.globl second
.type second,@function
second: call second_target
.byte 0xde,0xe9
ret
.size second,.-second
''')

    def assemble(self, name, source):
        path = self.root / (name + '.s')
        obj = path.with_suffix('.o')
        path.write_text(source)
        subprocess.run(['as', '--32', '-o', str(obj), str(path)], check=True)
        return obj

    def run_dis(self, *args, obj=None):
        return subprocess.run(['python3', str(DIS), str(obj or self.object),
                               *args], capture_output=True, text=True)

    def test_named_comdat_has_only_its_own_instructions_and_relocations(self):
        result = self.run_dis('first')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('R_386_PC32 first_target', result.stdout)
        self.assertNotIn('second_target', result.stdout)
        self.assertNotIn('text_target', result.stdout)
        self.assertEqual(result.stdout.count('call'), 1)
        self.assertIn('[1 relocation(s)', result.stderr)

    def test_intel_annotation_uses_same_section(self):
        result = self.run_dis('first')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('Intel: fsubrp', result.stdout)
        self.assertNotIn('Intel: fsubp', result.stdout)

    def test_numeric_range_defaults_to_text(self):
        result = self.run_dis('0x0', '0x6')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('R_386_PC32 text_target', result.stdout)
        self.assertNotIn('first_target', result.stdout)
        self.assertNotIn('second_target', result.stdout)

    def test_numeric_range_can_select_another_section(self):
        result = self.run_dis('0x0', '0x8', '--section', '.gnu.linkonce.t.second')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('R_386_PC32 second_target', result.stdout)
        self.assertNotIn('first_target', result.stdout)

    def test_duplicate_local_symbol_requires_a_section(self):
        objects = [self.assemble(name, '''.section .text.%s,"ax",@progbits
.type duplicate,@function
duplicate: ret
.size duplicate,.-duplicate
''' % name) for name in ('left', 'right')]
        joined = self.root / 'joined.o'
        subprocess.run(['ld', '-m', 'elf_i386', '-r', '-o', str(joined),
                        *(str(p) for p in objects)], check=True)
        result = self.run_dis('duplicate', obj=joined)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('ambiguous', result.stderr)
        result = self.run_dis('duplicate', '--section', '.text.right', obj=joined)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout.count('ret'), 1)

    def test_relocation_bearing_padding_shape_is_not_hidden(self):
        obj = self.assemble('padding', '''.text
.globl reloc_padding
.type reloc_padding,@function
reloc_padding:
.byte 0x8d,0xb6
.long padding_target
ret
.size reloc_padding,.-reloc_padding
''')
        result = self.run_dis('reloc_padding', obj=obj)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('R_386_32 padding_target', result.stdout)
        self.assertIn('lea', result.stdout)

    def test_missing_input_and_symbol_fail(self):
        self.assertNotEqual(self.run_dis('missing').returncode, 0)
        self.assertNotEqual(self.run_dis('first', obj=self.root/'missing.o').returncode, 0)

    def test_empty_range_reports_its_zero_denominator(self):
        result = self.run_dis('0x100', '0x110')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout.strip(), '')
        self.assertIn('0 instruction(s) decoded', result.stderr)

    def test_long_instruction_retains_both_relocations(self):
        obj = self.assemble('long', '''.text
.globl long_instruction
.type long_instruction,@function
long_instruction:
.byte 0xc7,0x05
.long address_target
.long value_target
ret
.size long_instruction,.-long_instruction
''')
        result = self.run_dis('long_instruction', obj=obj)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('R_386_32 address_target', result.stdout)
        self.assertIn('R_386_32 value_target', result.stdout)
        self.assertIn('2 instruction(s) decoded', result.stderr)


if __name__ == '__main__':
    unittest.main()
