#!/usr/bin/env python3
"""Detector controls; temporary files only, no reconstruction changes."""
import copy
import os
import json
from pathlib import Path
import sys
import tempfile
_tools = str(Path(__file__).resolve().parent)
sys.path.remove(_tools)
import dis
import unittest
sys.path.insert(0, _tools)
import recovery_experiment as ledger


class Session:
    def compare(self, baseline, candidate, reference):
        return {'shared_sized_functions': 1, 'grade_changes': {'EXACT': {
            'gained': ['target'] if Path(candidate).read_text() == 'AB' else [], 'lost': []}},
                'inventory': {'missing': [], 'added': []}}


class Controls(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.input = self.root / 'native'
        self.input.write_text('native')
        self.m = {'schema': 'recovery-experiment-v1', 'id': 'control',
                  'hypothesis': 'AB interaction', 'symbol': 'target', 'tu': 'fixture.cpp',
                  'native_witness': 'fixture negative control',
                  'compiler': {'identity': 'synthetic test only', 'flags': []},
                  'components': {'A': 'first', 'B': 'second'},
                  'fixture_domain': 'synthetic detector control',
                  'negative_controls': ['stale', 'failed', 'missing'],
                  'prior_attempts': [], 'reopen_rationale': '',
                  'inputs': [{'path': str(self.input), 'sha256': ledger.digest(self.input)}],
                  'reference': str(self.input), 'cells': {}}
        for name, components in [('baseline', []), ('A', ['A']), ('B', ['B']), ('AB', ['A', 'B'])]:
            obj = self.root / name
            obj.write_text(name)
            self.m['inputs'].append({'path': str(obj), 'sha256': ledger.digest(obj)})
            self.m['cells'][name] = {'components': components, 'object': str(obj), 'commands': []}

    def run_record(self):
        return ledger.run(self.m, self.root / 'ledger', Session())

    def test_crossed_cells_and_append_only(self):
        record = self.run_record()
        self.assertTrue(record['interaction']['complete'])
        self.assertEqual(record['cells']['AB']['comparison']['grade_changes']['EXACT']['gained'], ['target'])
        self.assertEqual(record['cells']['A']['comparison']['grade_changes']['EXACT']['gained'], [])
        self.assertEqual(record['cells']['AB']['period']['status'], 'not-run')
        with self.assertRaises(FileExistsError):
            self.run_record()
        self.assertEqual(len(ledger.query(self.root / 'ledger', symbol='target')), 1)
        self.assertEqual(ledger.query(self.root / 'ledger', tu='other'), [])

    def test_stale_and_missing_fail_closed(self):
        self.input.write_text('changed')
        with self.assertRaisesRegex(ValueError, 'stale'):
            self.run_record()
        self.input.unlink()
        with self.assertRaisesRegex(ValueError, 'missing'):
            self.run_record()

    def test_failed_command_and_skipped_dependents(self):
        self.m['cells']['A']['commands'] = [{'argv': [sys.executable, '-c', 'raise SystemExit(7)']},
                                            {'argv': [sys.executable, '-c', 'print("never")']}]
        self.m['cells']['A']['evidence'] = {'period': {'command': 0, 'result': 'absent'}}
        row = self.run_record()['cells']['A']
        self.assertEqual(row['commands'][0]['exit'], 7)
        self.assertEqual(row['commands'][1]['status'], 'not-run')
        self.assertEqual(row['period']['status'], 'failed')
        self.assertEqual(row['comparison']['status'], 'not-run')

    def test_no_shell_and_empty_denominator(self):
        output = self.root / 'empty.json'
        self.m['cells']['A']['commands'] = [{'argv': [sys.executable, '-c',
            'import pathlib; pathlib.Path(' + repr(str(output)) + ').write_text(\'{"passed":0,"total":0}\')'],
            'outputs': [str(output)]}]
        self.m['cells']['A']['evidence'] = {'period': {'command': 0, 'result': str(output)}}
        self.assertEqual(self.run_record()['cells']['A']['period']['status'], 'unknown')

    def test_success_command_does_not_prove_evidence(self):
        self.m['cells']['A']['commands'] = [{'argv': [sys.executable, '-c', 'pass']}]
        row = self.run_record()['cells']['A']
        self.assertEqual(row['commands'][0]['status'], 'passed')
        self.assertEqual(row['period']['status'], 'not-run')
        self.assertEqual(row['linked_body']['status'], 'not-run')

    def test_bounded_evidence_and_missing_executable(self):
        result = self.root / 'counts.json'
        self.m['cells']['A']['commands'] = [{'argv': [sys.executable, '-c',
            'import pathlib; pathlib.Path(' + repr(str(result)) + ').write_text(\'{"passed":3,"total":3}\')'],
            'outputs': [str(result)]}]
        self.m['cells']['A']['evidence'] = {'period': {'command': 0, 'result': str(result)}}
        self.m['cells']['B']['commands'] = [{'argv': [str(self.root / 'missing-executable')]}]
        rows = self.run_record()['cells']
        self.assertEqual(rows['A']['period']['total'], 3)
        self.assertEqual(rows['A']['period']['status'], 'passed')
        self.assertEqual(rows['B']['commands'][0]['status'], 'failed')
        self.assertEqual(rows['B']['comparison']['status'], 'not-run')

    def test_later_staleness_and_log_tamper(self):
        self.m['cells']['A']['commands'] = [{'argv': [sys.executable, '-c', 'print("control")']}]
        record = self.run_record()
        Path(record['cells']['A']['commands'][0]['log']).write_text('tampered')
        self.input.write_text('changed')
        errors = ledger.query(self.root / 'ledger')[0]['freshness_errors']
        self.assertEqual(len(errors), 2)

    def test_reopen_and_components_required(self):
        self.m['prior_attempts'] = ['prior']
        with self.assertRaisesRegex(ValueError, 'rationale'):
            self.run_record()
        self.m['reopen_rationale'] = 'new witness'
        self.m['cells']['AB']['components'] = ['A']
        with self.assertRaisesRegex(ValueError, 'components'):
            self.run_record()

    def test_generated_baseline_mutation_invalidates_crossing(self):
        generated = self.root / 'generated-baseline'
        self.m['cells']['baseline']['object'] = str(generated)
        self.m['cells']['baseline']['commands'] = [{'argv': [sys.executable, '-c',
            'from pathlib import Path; Path(' + repr(str(generated)) + ').write_text("baseline")'],
            'outputs': [str(generated)]}]
        self.m['cells']['A']['commands'] = [{'argv': [sys.executable, '-c',
            'from pathlib import Path; Path(' + repr(str(generated)) + ').write_text("changed")']}]
        record = self.run_record()
        self.assertFalse(record['interaction']['complete'])
        self.assertTrue(record['freshness_errors'])
        self.assertEqual(record['cells']['A']['comparison']['status'], 'not-run')

    def test_query_from_another_directory(self):
        previous = Path.cwd()
        try:
            os.chdir(self.root)
            for item in self.m['inputs']:
                item['path'] = Path(item['path']).name
            self.m['reference'] = 'native'
            for cell in self.m['cells'].values():
                cell['object'] = Path(cell['object']).name
            self.run_record()
        finally:
            os.chdir(previous)
        self.assertEqual(ledger.query(self.root / 'ledger')[0]['freshness_errors'], [])

    def test_analyzer_change_makes_retrieved_evidence_stale(self):
        analyzer = self.root / 'analyzer.py'
        analyzer.write_text('version one')
        session = Session()
        session._TOOL_INPUTS = [analyzer]
        ledger.run(self.m, self.root / 'ledger', session)
        analyzer.write_text('version two')
        errors = ledger.query(self.root / 'ledger')[0]['freshness_errors']
        self.assertEqual(errors, ['stale: ' + str(analyzer)])


if __name__ == '__main__':
    unittest.main()
