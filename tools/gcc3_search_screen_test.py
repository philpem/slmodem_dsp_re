#!/usr/bin/env python3
"""Synthetic screen controls; these do not establish compiler reachability."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile

TOOLS = Path(__file__).resolve().parent
sys.path[:] = [p for p in sys.path if Path(p or '.').resolve() != TOOLS]
import unittest
from unittest.mock import patch
sys.path.insert(0, str(TOOLS))
import gcc3_search_screen as screen


def event(name, eligible, before, selected, after):
    return {'function': name, 'constraint': 'r', 'mode': 'SImode', 'from': 0, 'to': 1,
            'allocation_order': list(range(53)), 'excluded': [],
            'cursor_before': before, 'selected': selected, 'cursor_after': after,
            'candidates': [{'order_index': before, 'register': before}],
            'eligibility': {'class': 12, 'class_members': eligible, 'live': [],
                            'fixed': [0] * 53, 'call_used': [1] * 53, 'ever_live': [0] * 53,
                            'reload_completed': 1, 'frame_pointer_needed': 0}}


def trace():
    return {'events': [event('sensitive', [0, 4], 0, 0, 1),
                       event('invariant', [1], 1, 1, 2),
                       event('unknown', [2], 2, 2, 3)],
            'errors': [], 'exit_codes': [0], 'pending': 0}


class ScreenTest(unittest.TestCase):
    def test_aggregation_absent_function_unknown_and_all_denominators(self):
        evidence = trace()
        status, calibrated = screen.screen_trace(evidence)
        grades = {'sensitive': ['BYTES', 4], 'invariant': ['SIZE', 2],
                  'absent': ['UNRESOLVED', 0], 'exact': ['EXACT', 0]}
        result = screen.classify_functions(grades, evidence['events'], calibrated,
                                           list(grades) + ['unknown'])
        self.assertEqual(status['modeled_events'], 3)
        self.assertEqual(result['shared_sized_functions'], 4)
        self.assertEqual(result['nonexact_functions'], 3)
        self.assertEqual(result['nonexact_with_observed_events'], ['invariant', 'sensitive'])
        self.assertEqual(result['nonexact_without_observed_events'], ['absent'])
        self.assertEqual(result['nonexact_cursor_sensitive'], ['sensitive'])
        self.assertEqual(result['event_functions_without_shared_blob_function'], ['unknown'])
        self.assertEqual(result['event_functions_without_sized_definition'], [])
        rows = {r['symbol']: r for r in result['functions']}
        self.assertEqual(rows['absent']['cursor_diagnostic'], 'no_observed_events')
        self.assertEqual(rows['sensitive']['modes'], {'SImode': 1})

    def test_sensitive_and_invariant_single_events_have_hand_derived_signatures(self):
        evidence = trace()
        _, calibrated = screen.screen_trace(evidence)
        sensitive = screen.block_sensitivity(calibrated['blocks'][0])
        invariant = screen.block_sensitivity(calibrated['blocks'][1])
        self.assertEqual(sensitive['incoming_cursor_states'], 53)
        self.assertEqual(sensitive['distinct_choice_signatures'], 2)
        self.assertEqual(sensitive['signatures'], [
            {'selected': [0], 'starts': [0] + list(range(5, 53))},
            {'selected': [4], 'starts': [1, 2, 3, 4]}])
        self.assertEqual(invariant['distinct_choice_signatures'], 1)
        self.assertFalse(invariant['sensitive'])
        self.assertEqual(invariant['signatures'], [{'selected': [1], 'starts': list(range(53))}])

    def test_all_unsupported_events_are_retained_and_disable_every_prediction(self):
        evidence = trace()
        evidence['events'][0]['mode'] = 'DImode'
        evidence['events'][2]['constraint'] = 'q'
        with patch.object(screen.model_api, 'calibrate', side_effect=AssertionError('must not model prefix')):
            status, calibrated = screen.screen_trace(evidence)
        self.assertIsNone(calibrated)
        self.assertEqual(status['status'], 'unsupported_event_census')
        self.assertEqual((status['observed_events'], status['supported_events'], status['modeled_events']),
                         (3, 1, 0))
        self.assertEqual(status['locally_checked_supported_decisions'], 1)
        self.assertEqual([r['index'] for r in status['unsupported_events']], [0, 2])
        result = screen.classify_functions({'invariant': ['BYTES', 1]}, evidence['events'])
        self.assertEqual(result['functions'][0]['cursor_diagnostic'], 'unavailable_whole_stream_unsupported')
        self.assertEqual(result['functions'][0]['blocks'], [])
        self.assertIsNone(result['nonexact_cursor_sensitive'])
        self.assertEqual(result['cursor_diagnostic_nonexact_function_denominator'], 0)

    def test_empty_denominator_refused(self):
        with self.assertRaisesRegex(screen.ScreenError, 'denominator.*0'):
            screen.classify_functions({}, trace()['events'])

    def test_corrupt_supported_event_and_bad_envelope_refused(self):
        evidence = trace()
        evidence['events'][1]['selected'] = 4
        with self.assertRaisesRegex(screen.ScreenError, 'selection/cursor'):
            screen.screen_trace(evidence)
        evidence = trace()
        evidence['errors'] = ['incomplete observation']
        with self.assertRaisesRegex(screen.ScreenError, 'incomplete'):
            screen.screen_trace(evidence)
        evidence = trace()
        del evidence['events'][1]['eligibility']['fixed']
        with self.assertRaisesRegex(screen.ScreenError, 'missing mechanism'):
            screen.screen_trace(evidence)
        evidence = trace()
        evidence['events'][0]['mode'] = 'DImode'
        evidence['exit_codes'] = [False]
        with self.assertRaisesRegex(screen.ScreenError, 'non-integer'):
            screen.screen_trace(evidence)

    def test_unsupported_mode_still_requires_complete_valid_common_observations(self):
        evidence = trace()
        evidence['events'][0]['mode'] = 'QImode'
        status, calibrated = screen.screen_trace(evidence)
        self.assertIsNone(calibrated)
        self.assertEqual(status['unsupported_events'][0]['mode'], 'QImode')
        self.assertEqual(status['locally_checked_supported_decisions'], 2)
        changes = [('cursor_before', lambda row: row.__setitem__('cursor_before', 'bad')),
                   ('eligibility', lambda row: row['eligibility'].__setitem__('fixed', [0])),
                   ('candidate', lambda row: row['candidates'][0].__setitem__('order_index', 53)),
                   ('selected', lambda row: row.__setitem__('selected', 53))]
        for label, change in changes:
            evidence = trace()
            evidence['events'][0]['mode'] = 'QImode'
            change(evidence['events'][0])
            with self.subTest(label=label), self.assertRaises((screen.ScreenError, screen.model_api.ModelError)):
                screen.screen_trace(evidence)

    def test_supported_corrupt_outcome_is_refused_inside_unsupported_stream(self):
        evidence = trace()
        evidence['events'][0]['mode'] = 'QImode'
        evidence['events'][1]['selected'] = 4
        with self.assertRaisesRegex(screen.ScreenError, 'selection/cursor.*local supported'):
            screen.screen_trace(evidence)

    def test_mixed_hi_si_stream_is_fully_modeled_with_modes_preserved(self):
        evidence = trace()
        evidence['events'][0]['mode'] = 'HImode'
        status, calibrated = screen.screen_trace(evidence)
        self.assertEqual(status['status'], 'calibrated_whole_stream')
        self.assertEqual((status['observed_events'], status['modeled_events']), (3, 3))
        self.assertEqual(status['locally_checked_supported_decisions'], 3)
        result = screen.classify_functions({'sensitive': ['BYTES', 1]}, evidence['events'], calibrated)
        self.assertEqual(result['functions'][0]['modes'], {'HImode': 1})
        self.assertEqual(result['nonexact_cursor_sensitive'], ['sensitive'])

    def test_corrupt_hi_outcome_is_refused_inside_unsupported_stream(self):
        evidence = trace()
        evidence['events'][0]['mode'] = 'HImode'
        evidence['events'][0]['selected'] = 4
        evidence['events'][2]['mode'] = 'QImode'
        with self.assertRaisesRegex(screen.ScreenError, 'selection/cursor.*local supported'):
            screen.screen_trace(evidence)

    def fixture(self, directory):
        root = Path(directory)
        cap = root / 'capture'
        (cap / 'initial').mkdir(parents=True)
        (cap / 'repeat').mkdir()
        source = root / 'source.c'
        source.write_text('int f(void) { return 1; }\n')
        (cap / 'source-input.c').write_bytes(source.read_bytes())
        config = root / '.build-config'
        config.write_text('image test-image\nflags -O3 -DDSPLIB_REPRODUCE_BUGS\n')
        (cap / '.build-config').write_bytes(config.read_bytes())
        compiler = cap / 'cc1'
        compiler.write_bytes(b'synthetic compiler')
        for target in ('initial/candidate.o', 'repeat/candidate.o', 'observed.o'):
            (cap / target).write_bytes(b'synthetic object')
        for target in ('initial/source.i', 'source.i'):
            (cap / target).write_bytes(b'synthetic preprocessed input')
        for target in ('initial/source.s', 'plain.s', 'observed.s'):
            (cap / target).write_bytes(b'synthetic assembly')
        baseline = root / 'baseline.o'
        baseline.write_bytes((cap / 'observed.o').read_bytes())
        result = {'schema': 1, 'status': 'valid', 'root': str(root), 'source': str(source),
                  'source_sha256': screen.sha(source),
                  'capture_sha256': screen.sha(TOOLS / 'gcc3_search_capture.py'),
                  'observer_sha256': screen.sha(TOOLS / 'gcc3_search_capture_gdb.py'),
                  'checks': {k: True for k in ('repeated_object_equal', 'observed_object_equal',
                                              'initial_plain_observed_assembly_equal')},
                  'commands': [{'returncode': 0}], 'config_path': str(config),
                  'config_sha256': screen.sha(config), 'config': config.read_text(),
                  'header_sha256': {}, 'source_parent_header_sha256': {},
                  'compiler_kind': 'cc1', 'compiler_sha256': screen.sha(compiler),
                  'object': 'initial/candidate.o', 'object_sha256': screen.sha(baseline),
                  'preprocessed_sha256': screen.sha(cap / 'source.i'),
                  'assembly_sha256': screen.sha(cap / 'plain.s'),
                  'trace': 'scratch-observe.json', 'image_id': 'synthetic-image',
                  'effective_flags': ['-O3', '-DDSPLIB_REPRODUCE_BUGS'],
                  'events': 3, 'functions_with_events': 3}
        evidence = trace()
        evidence['provenance'] = {k: result[k] for k in ('compiler_sha256', 'image_id',
                                'effective_flags', 'observer_sha256', 'capture_sha256')}
        (cap / 'scratch-observe.json').write_text(json.dumps(evidence))
        (cap / 'results.json').write_text(json.dumps(result))
        return cap, baseline, result

    def test_valid_capture_and_known_corrupt_object_hash_control(self):
        with tempfile.TemporaryDirectory() as directory:
            cap, baseline, result = self.fixture(directory)
            with patch.dict(screen.capture_api.SUPPORTED, {'cc1': result['compiler_sha256']}), \
                 patch.object(screen.subprocess, 'run', return_value=subprocess.CompletedProcess([], 0, stdout='')):
                _, _, identity = screen.validate_capture(cap, baseline)
                self.assertTrue(identity['raw_baseline_object_equal'])
                self.assertTrue(identity['baseline_config_checked'])
                (cap / 'initial/candidate.o').write_bytes(b'known corruption')
                with self.assertRaisesRegex(screen.ScreenError, 'captured object: hash mismatch'):
                    screen.validate_capture(cap, baseline)

    def test_stale_source_and_provenance_negative_controls(self):
        with tempfile.TemporaryDirectory() as directory:
            cap, baseline, result = self.fixture(directory)
            original = Path(result['source']).read_bytes()
            Path(result['source']).write_bytes(b'changed source')
            with self.assertRaisesRegex(screen.ScreenError, 'current source: hash mismatch'):
                screen.validate_capture(cap, baseline)
            Path(result['source']).write_bytes(original)
            trace_path = cap / 'scratch-observe.json'
            evidence = json.loads(trace_path.read_text())
            evidence['provenance']['image_id'] = 'changed image'
            trace_path.write_text(json.dumps(evidence))
            with patch.dict(screen.capture_api.SUPPORTED, {'cc1': result['compiler_sha256']}), \
                 patch.object(screen.subprocess, 'run', return_value=subprocess.CompletedProcess([], 0, stdout='')):
                with self.assertRaisesRegex(screen.ScreenError, 'provenance mismatch'):
                    screen.validate_capture(cap, baseline)

    def test_failed_cli_writes_invalid_fresh_report_and_refuses_existing_output(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            output = root / 'screen.json'
            arguments = ['--capture', str(root / 'missing'), '--baseline-object', str(root / 'baseline.o'),
                         '--output', str(output)]
            self.assertEqual(screen.main(arguments), 1)
            result = json.loads(output.read_text())
            self.assertEqual(result['status'], 'invalid')
            self.assertIsNone(result['shared_sized_functions'])
            original = output.read_bytes()
            self.assertEqual(screen.main(arguments), 1)
            self.assertEqual(output.read_bytes(), original)


if __name__ == '__main__':
    unittest.main()
