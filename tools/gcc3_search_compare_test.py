#!/usr/bin/env python3
"""Real ELF positive/negative controls for the variant report; no compiler claims."""
from pathlib import Path
import subprocess
import sys
import tempfile
TOOLS = Path(__file__).resolve().parent
sys.path[:] = [p for p in sys.path if Path(p or '.').resolve() != TOOLS]
import unittest
sys.path.insert(0, str(TOOLS))
import gcc3_search_compare as report


class ComparisonTests(unittest.TestCase):
    def setUp(self):
        # Keep test artifacts in build, as with compiler experiments.
        root = TOOLS.parent / 'build'
        root.mkdir(exist_ok=True)
        self.tmp = tempfile.TemporaryDirectory(prefix='gcc3-compare-test-', dir=root)
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)

    def obj(self, name, bodies, data=1):
        source = self.root / (name + '.s')
        obj = self.root / (name + '.o')
        source.write_text('.text\n' + '\n'.join(
            '.globl %s\n.type %s,@function\n%s:\n%s\n.size %s,.-%s\n' % (n,n,n,b,n,n)
            for n,b in bodies) + '\n.data\n.long %d\n' % data)
        subprocess.run(['as', '--32', '-o', str(obj), str(source)], check=True, capture_output=True)
        return obj

    def baseline(self):
        return self.obj('base', [('f', 'mov $1,%eax\nmov %eax,(%edx)\nret'), ('g', 'ret')])

    def test_register_rename_is_regalloc_and_raw_bytes_remain_visible(self):
        base = self.baseline()
        variant = self.obj('variant', [('f', 'mov $1,%ecx\nmov %ecx,(%edx)\nret'), ('g', 'ret')])
        out = report.compare(base, variant, base)
        self.assertEqual(out['shared_sized_functions'], 2)
        self.assertEqual(out['functions']['f']['candidate']['grade'], ['REGALLOC', 0])
        self.assertEqual(out['functions']['f']['candidate']['raw'][0], 'BYTES')
        self.assertEqual(out['grade_changes']['EXACT']['lost'], ['f'])

    def test_changed_immediate_is_not_regalloc(self):
        base = self.baseline()
        variant = self.obj('variant', [('f', 'mov $2,%eax\nmov %eax,(%edx)\nret'), ('g', 'ret')])
        self.assertEqual(report.compare(base, variant, base)['functions']['f']['candidate']['grade'][0], 'BYTES')

    def test_body_gain_and_layout_difference_are_independent(self):
        reference = self.baseline()
        base = self.obj('old', [('f', 'mov $1,%ecx\nmov %ecx,(%edx)\nret'), ('g', 'ret')])
        variant = self.obj('variant', [('g', 'ret'), ('f', 'mov $1,%eax\nmov %eax,(%edx)\nret')])
        out = report.compare(base, variant, reference)
        self.assertEqual(out['grade_changes']['EXACT']['gained'], ['f'])
        self.assertTrue(out['layout']['baseline_matches_reference'])
        self.assertFalse(out['layout']['candidate_matches_reference'])
        self.assertTrue(out['exports_unchanged'])

    def test_changed_relocation_is_not_regalloc(self):
        base = self.obj('base', [('f', 'call original\nret')])
        variant = self.obj('variant', [('f', 'call different\nret')])
        self.assertEqual(report.compare(base, variant, base)['functions']['f']['candidate']['grade'][0], 'RELOC')

    def test_inventory_data_and_zero_denominator(self):
        base = self.baseline()
        variant = self.obj('variant', [('f', 'ret'), ('added', 'ret')], data=2)
        out = report.compare(base, variant, base)
        self.assertEqual(out['shared_sized_functions'], 1)
        self.assertEqual(out['inventory']['missing'], ['g'])
        self.assertEqual(out['inventory']['added'], ['added'])
        self.assertFalse(out['allocated_data_raw_unchanged'])
        self.assertFalse(out['exports_unchanged'])
        empty = self.obj('empty', [('other', 'ret')])
        with self.assertRaisesRegex(ValueError, 'denominator is 0'):
            report.compare(base, empty, base)

    def test_invalid_report_and_no_overwrite(self):
        output = self.root / 'report.json'
        args = ['--baseline', str(self.root/'absent'), '--candidate', str(self.root/'absent'), '--output', str(output)]
        self.assertEqual(report.main(args), 1)
        text = output.read_text()
        self.assertIn('"shared_sized_functions": null', text)
        self.assertEqual(report.main(args), 1)
        self.assertEqual(output.read_text(), text)

    def test_invalid_elf_reports_failure(self):
        bad = self.root / 'bad.o'
        bad.write_text('not an ELF object')
        base = self.baseline()
        output = self.root / 'invalid-elf.json'
        self.assertEqual(report.main(['--baseline', str(base), '--candidate', str(bad),
                                      '--blob', str(base), '--output', str(output)]), 1)
        self.assertIn('"status": "invalid"', output.read_text())

    def test_aliases_are_grouped_without_inventing_order(self):
        source = self.root / 'aliases.s'
        obj = self.root / 'aliases.o'
        source.write_text('.text\n.globl f, alias\n.type f,@function\n.type alias,@function\n'
                          'f:\nalias:\nret\n.size f,.-f\n.size alias,.-alias\n')
        subprocess.run(['as', '--32', '-o', str(obj), str(source)], check=True, capture_output=True)
        self.assertEqual(report.layout(str(obj), ['f', 'alias']), {'.text': [['alias', 'f']]})


if __name__ == '__main__':
    unittest.main()
