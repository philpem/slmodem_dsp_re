#!/usr/bin/env python3
"""Real ELF controls for immediate-store width screening."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile

TOOLS = Path(__file__).resolve().parent
sys.path[:] = [p for p in sys.path if Path(p or '.').resolve() != TOOLS]
import unittest
from unittest import mock
sys.path.insert(0, str(TOOLS))
import gcc3_search_width as width


class WidthScreenTests(unittest.TestCase):
    def setUp(self):
        root = TOOLS.parent / 'build'
        root.mkdir(exist_ok=True)
        self.tmp = tempfile.TemporaryDirectory(prefix='gcc3-width-test-', dir=root)
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)

    def object(self, name, body, symbol='f'):
        source = self.root / (name + '.s')
        obj = self.root / (name + '.o')
        source.write_text('.text\n.globl %s\n.type %s,@function\n%s:\n%s\n.size %s,.-%s\n' %
                          (symbol, symbol, symbol, body, symbol, symbol))
        subprocess.run(['as', '--32', '-o', str(obj), str(source)],
                       check=True, capture_output=True)
        return obj

    def test_word_to_long_is_nominated(self):
        native = self.object('native', 'movw $7,4(%eax)\nret')
        candidate = self.object('candidate', 'movl $7,4(%eax)\nret')
        report = width.analyze(native, candidate, ['f'])
        finding = report['functions'][0]['findings'][0]
        self.assertEqual(report['denominator']['selected_functions'], 1)
        self.assertEqual(report['status'], 'valid')
        self.assertEqual(report['analysis_script']['sha256'], width.sha256(TOOLS / 'gcc3_search_width.py'))
        self.assertEqual(finding['signature'], {'immediate': 7, 'destination': '0x4(%eax)'})
        self.assertEqual(finding['unmatched_native_narrow'], {'b': 0, 'w': 1})
        self.assertEqual(finding['unmatched_candidate_wide'], {'l': 1, 'd': 0})
        self.assertFalse(finding['owner_proven'])
        self.assertEqual(report['inputs']['native']['sha256'], width.sha256(native))

    def test_identical_word_and_long_stores_cancel(self):
        body = 'movw $7,4(%eax)\nmovl $7,4(%eax)\nret'
        native = self.object('native', body)
        candidate = self.object('candidate', body)
        report = width.analyze(native, candidate, ['f'])
        self.assertEqual(report['candidate_count'], 0)
        self.assertEqual(report['functions'][0]['findings'], [])

    def test_unequal_residual_multiplicities_do_not_nominate(self):
        native = self.object('native', 'movw $7,4(%eax)\nmovw $7,4(%eax)\nret')
        candidate = self.object('candidate', 'movl $7,4(%eax)\nret')
        self.assertEqual(width.analyze(native, candidate, ['f'])['candidate_count'], 0)

    def test_changed_constant_or_destination_does_not_match(self):
        native = self.object('native', 'movw $7,4(%eax)\nret')
        different_constant = self.object('different-constant', 'movl $8,4(%eax)\nret')
        different_displacement = self.object('different-displacement', 'movl $7,8(%eax)\nret')
        different_base = self.object('different-base', 'movl $7,4(%ebx)\nret')
        self.assertEqual(width.analyze(native, different_constant, ['f'])['candidate_count'], 0)
        self.assertEqual(width.analyze(native, different_displacement, ['f'])['candidate_count'], 0)
        self.assertEqual(width.analyze(native, different_base, ['f'])['candidate_count'], 0)

    def test_mangled_symbol_is_scanned_and_max_bytes_limits_denominator(self):
        symbol = '_Z5methodv'
        native = self.object('native-cxx', 'movw $3,0x8(%ebp)\nret', symbol)
        candidate = self.object('candidate-cxx', 'movl $3,0x8(%ebp)\nret', symbol)
        included = width.analyze(native, candidate, max_bytes=16)
        self.assertEqual(included['denominator']['selected_functions'], 1)
        self.assertEqual(included['candidate_count'], 1)
        with self.assertRaisesRegex(ValueError, 'denominator is 0'):
            width.analyze(native, candidate, max_bytes=1)

    def test_vmi_v17_v29_frozen_baseline_positive_current_clear(self):
        root = TOOLS.parent
        blob = root / 'ref/slmodemd/dsplibs.o'
        baseline = root / 'build/gcc3-overnight/baseline/src_fax_class1tx.c.o'
        current = root / 'build/tc_out/src_fax_class1tx.c.o'
        if not all(path.is_file() for path in (blob, baseline, current)):
            self.skipTest('frozen VMI baseline/current artifacts are unavailable')
        campaign_path = root / 'build/gcc3-overnight/campaign.json'
        if not campaign_path.is_file():
            self.skipTest('frozen campaign hash record is unavailable')
        expected_hash = json.loads(campaign_path.read_text())['baseline_objects'].get(baseline.name)
        if expected_hash is None or width.sha256(baseline) != expected_hash:
            self.skipTest('VMI baseline object does not match its frozen campaign hash')
        for symbol in ('init_vmi_v17tx', 'init_vmi_v29tx'):
            with self.subTest(symbol=symbol):
                old = width.analyze(blob, baseline, [symbol])
                now = width.analyze(blob, current, [symbol])
                self.assertGreater(old['candidate_count'], 0)
                self.assertEqual(now['candidate_count'], 0)
                self.assertEqual(old['inputs']['candidate']['sha256'], expected_hash)

    def test_cli_writes_provenance_report_without_overwrite(self):
        native = self.object('native', 'movw $7,4(%eax)\nret')
        candidate = self.object('candidate', 'movl $7,4(%eax)\nret')
        output = self.root / 'report.json'
        argv = [sys.executable, str(TOOLS / 'gcc3_search_width.py'), '--native', str(native),
                '--candidate', str(candidate), '--symbol', 'f', '--max-bytes', '16',
                '--output', str(output)]
        subprocess.run(argv, check=True, capture_output=True, text=True)
        saved = json.loads(output.read_text())
        self.assertEqual(saved['denominator']['selected_functions'], 1)
        self.assertEqual(saved['candidate_count'], 1)
        refused = subprocess.run(argv, capture_output=True, text=True)
        self.assertNotEqual(refused.returncode, 0)
        self.assertEqual(json.loads(output.read_text()), saved)

        original_bytes = candidate.read_bytes()
        original_stores = width.immediate_memory_stores

        def mutate_candidate(path, symbol):
            rows = original_stores(path, symbol)
            if Path(path).resolve() == candidate.resolve():
                candidate.write_bytes(candidate.read_bytes() + b'mutated')
            return rows

        with mock.patch.object(width, 'immediate_memory_stores', side_effect=mutate_candidate):
            with self.assertRaisesRegex(ValueError, 'input changed during width analysis'):
                width.analyze(native, candidate, ['f'])
        candidate.write_bytes(original_bytes)


if __name__ == '__main__':
    unittest.main()
