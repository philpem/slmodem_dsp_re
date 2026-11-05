#!/usr/bin/env python3
"""Real ELF controls distinguishing payload, owner and target equality."""
from pathlib import Path
import subprocess
import sys
import tempfile

TOOLS = Path(__file__).resolve().parent
sys.path[:] = [p for p in sys.path if Path(p or '.').resolve() != TOOLS]
import unittest
from elftools.elf.elffile import ELFFile
sys.path.insert(0, str(TOOLS))
import gcc3_search_data as audit


class DataTests(unittest.TestCase):
    def setUp(self):
        root = TOOLS.parent / 'build'
        root.mkdir(exist_ok=True)
        self.tmp = tempfile.TemporaryDirectory(prefix='gcc3-data-test-', dir=root)
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)

    def obj(self, name, target='f+5', prefix='', payload=7, alias=False, relocation=''):
        source, obj = self.root / (name + '.s'), self.root / (name + '.o')
        source.write_text('.text\n' + prefix + '\n.globl f\n.type f,@function\nf:\n'
                          'mov $1,%eax\nret\n.size f,.-f\n' +
                          ('.globl alias\n.type alias,@function\n.set alias,f\n.size alias,6\n' if alias else '') +
                          '.section .rodata\n.long ' + target + '\n.long %d\n' % payload + relocation)
        subprocess.run(['as', '--32', '-o', str(obj), str(source)], check=True, capture_output=True)
        return obj

    def test_relocated_function_keeps_exact_relative_target(self):
        a = self.obj('a')
        b = self.obj('b', prefix='.space 16,0x90')
        r = audit.compare(a, b)
        self.assertTrue(r['masked_payload_and_layout_unchanged'])
        self.assertTrue(r['strict_relocation_targets_unchanged'])
        self.assertEqual(r['code_targets']['compared'], 1)
        self.assertEqual(r['code_targets']['changed'], 0)

    def test_different_valid_case_keeps_owner_but_changes_target(self):
        r = audit.compare(self.obj('a'), self.obj('b', target='f'))
        self.assertTrue(r['masked_payload_and_layout_unchanged'])
        self.assertFalse(r['strict_relocation_targets_unchanged'])
        self.assertEqual(r['code_targets']['changed'], 1)
        self.assertEqual(r['code_targets']['owner_changes'], 0)

    def test_interior_byte_is_unresolved_even_when_identical(self):
        a = self.obj('a', target='f+1')
        r = audit.compare(a, a)
        self.assertIsNone(r['strict_relocation_targets_unchanged'])
        self.assertEqual(len(r['unresolved_relocation_fields']), 1)
        self.assertIn('instruction boundary', r['unresolved_relocation_fields'][0]['baseline']['reason'])

    def test_ambiguous_function_alias_is_unresolved(self):
        a = self.obj('a', alias=True)
        r = audit.compare(a, a)
        self.assertIsNone(r['strict_relocation_targets_unchanged'])
        self.assertIn('2 function owners', r['unresolved_relocation_fields'][0]['baseline']['reason'])

    def test_nonrelocated_payload_change_is_not_hidden(self):
        r = audit.compare(self.obj('a'), self.obj('b', payload=8))
        self.assertFalse(r['masked_payload_and_layout_unchanged'])
        self.assertTrue(r['strict_relocation_targets_unchanged'])

    def test_external_symbol_addend_is_retained(self):
        r = audit.compare(self.obj('a', target='external+1'), self.obj('b', target='external+2'))
        self.assertFalse(r['strict_relocation_targets_unchanged'])
        self.assertEqual(r['code_targets']['compared'], 0)

    def test_unknown_section_destination_is_unresolved(self):
        a = self.obj('a', target='.rodata+4')
        self.assertIsNone(audit.compare(a, a)['strict_relocation_targets_unchanged'])

    def test_overlapping_relocation_fields_are_rejected(self):
        a = self.obj('a', relocation='.reloc .rodata+1,R_386_32,f\n')
        with self.assertRaisesRegex(ValueError, 'overlapping'):
            audit.compare(a, a)

    def test_zero_relocation_denominator_is_explicit(self):
        a = self.obj('a', target='0')
        r = audit.compare(a, a)
        self.assertEqual(r['relocation_fields'], {'baseline': 0, 'candidate': 0})
        self.assertEqual(r['code_targets']['compared'], 0)

    def test_duplicate_local_function_names_do_not_identify_a_target(self):
        inputs = []
        for name, value in [('a', 1), ('b', 2)]:
            source, obj = self.root / (name + '.s'), self.root / (name + '.o')
            source.write_text('.text\n.type local_f,@function\nlocal_f:\n'
                              'mov $%d,%%eax\nret\n.size local_f,.-local_f\n'
                              '.data\n.long local_f\n' % value)
            subprocess.run(['as', '--32', '-o', str(obj), str(source)], check=True, capture_output=True)
            inputs.append(str(obj))
        joined = self.root / 'joined.o'
        subprocess.run(['ld', '-m', 'elf_i386', '-r', '-o', str(joined)] + inputs,
                       check=True, capture_output=True)
        r = audit.compare(joined, joined)
        self.assertEqual(r['relocation_fields']['baseline'], 2)
        self.assertIsNone(r['strict_relocation_targets_unchanged'])
        self.assertEqual(len(r['unresolved_relocation_fields']), 2)

    def test_absolute_symbol_is_not_an_ordinary_named_destination(self):
        source, obj = self.root / 'absolute.s', self.root / 'absolute.o'
        source.write_text('.globl ext\n.set ext,1\n.data\n.long 1\n'
                          '.reloc .data,R_386_32,ext\n')
        subprocess.run(['as', '--32', '-o', str(obj), str(source)], check=True, capture_output=True)
        self.assertIsNone(audit.compare(obj, obj)['strict_relocation_targets_unchanged'])

    def test_unbacked_section_payload_is_rejected(self):
        obj = self.obj('a', target='0')
        with obj.open('rb') as stream:
            elf = ELFFile(stream)
            field = elf['e_shoff'] + elf.get_section_index('.rodata') * elf['e_shentsize'] + 16
        raw = bytearray(obj.read_bytes())
        raw[field:field + 4] = (len(raw) + 100).to_bytes(4, 'little')
        obj.write_bytes(raw)
        with self.assertRaisesRegex(ValueError, 'truncated allocated section'):
            audit.compare(obj, obj)


if __name__ == '__main__':
    unittest.main()
