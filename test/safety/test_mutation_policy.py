#!/usr/bin/env python3
"""Controls for gate isolation and refusing implicit mutation campaigns."""
import importlib.util
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from contextlib import redirect_stderr
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('mutsnap_policy', ROOT/'tools/mutsnap.py')
mutsnap = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mutsnap)
anchor_spec = importlib.util.spec_from_file_location('anchor_policy', ROOT/'tools/anchorcheck.py')
anchorcheck = importlib.util.module_from_spec(anchor_spec)
anchor_spec.loader.exec_module(anchorcheck)


class MutationPolicy(unittest.TestCase):
    def test_anchor_search_preserves_overlapping_and_boundary_matches(self):
        for src, anchor in [('aaaa', 'aa'), ('abc', 'abc'), ('abc', 'c'),
                            ('abc', 'missing'), ('abc', ''), ('', ''),
                            ('a\na\n', 'a\n')]:
            with self.subTest(src=src, anchor=anchor):
                expected = [i for i in range(len(src)) if src.startswith(anchor, i)]
                self.assertEqual(anchorcheck.anchor_positions(src, anchor), expected)

    def test_anchor_detector_rejects_detached_ambiguous_and_vacuous_locators(self):
        for source, find, replace, marker in (
            ('return 1;', 'return 1;', 'return 2;', None),
            ('return 1;', 'return 3;', 'return 2;', 'NOT UNIQUE'),
            ('return 1; return 1;', 'return 1;', 'return 2;', 'NOT UNIQUE'),
            ('return 1;', 'return 1;', 'return 1;', 'VACUOUS'),
        ):
            with self.subTest(marker=marker), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp)
                (root/'test/mutations').mkdir(parents=True)
                (root/'source.c').write_text('int\nprobe(void)\n{\n'+source+'\n}\n')
                (root/'test/mutations/suites.json').write_text(json.dumps(
                    {'probe': ['source.c', 'build/test/probe']}))
                (root/'test/mutations/probe.json').write_text(json.dumps([
                    {'label': 'return value', 'find': find, 'replace': replace}]))
                result = subprocess.run([sys.executable, str(ROOT/'tools/anchorcheck.py')],
                                        cwd=root, capture_output=True, text=True)
                self.assertEqual(result.returncode, 1 if marker else 0, result.stderr)
                self.assertIn('1 suite(s) checked, 1 mutation(s), 0 skipped', result.stdout)
                if marker:
                    self.assertIn(marker, result.stdout)

    def test_execution_without_selection_cannot_start_a_campaign(self):
        for operation in ('--update', '--verify'):
            with self.subTest(operation=operation), \
                 patch.object(sys, 'argv', ['mutsnap.py', operation]), \
                 patch.object(mutsnap, 'run_suite') as run, \
                 redirect_stderr(io.StringIO()) as err:
                with self.assertRaises(SystemExit) as raised:
                    mutsnap.main()
                self.assertEqual(raised.exception.code, 2)
                self.assertIn('require explicit suite names', err.getvalue())
                run.assert_not_called()

    def test_named_execution_still_dispatches(self):
        for operation, handler in (('--update', 'cmd_update'), ('--verify', 'cmd_verify')):
            with self.subTest(operation=operation), \
                 patch.object(sys, 'argv', ['mutsnap.py', operation, 'pulse']), \
                 patch.object(mutsnap, handler, return_value=19) as run:
                self.assertEqual(mutsnap.main(), 19)
                self.assertEqual(run.call_args.args[0].suite, ['pulse'])

    def test_gate_graph_has_no_snapshot_or_mutant_execution(self):
        # make -p -q prints the expanded dependency database without recipes.
        result = subprocess.run(['make', '-p', '-q', 'phase-full'], cwd=ROOT,
                                capture_output=True, text=True)
        self.assertIn(result.returncode, (0, 1), result.stderr)
        targets = {}
        for line in result.stdout.splitlines():
            if line and not line[0].isspace() and ':' in line and ':=' not in line:
                name, deps = line.split(':', 1)
                if ' ' not in name:
                    targets.setdefault(name, set()).update(deps.split())
        def closure(target):
            seen, pending = set(), [target]
            while pending:
                name = pending.pop()
                if name not in seen:
                    seen.add(name)
                    pending.extend(targets.get(name, ()))
            return seen
        # phase/portability invoke these expanded tiers via recursive recipes.
        for gate, tiers in (('phase', 'PHASE_TIERS'), ('portability', 'PORTABILITY_TIERS'),
                            ('test', None), ('phase-full', None)):
            with self.subTest(gate=gate):
                selected = closure(gate)
                if tiers:
                    value = subprocess.check_output(['make', '-s', 'print-'+tiers],
                                                    cwd=ROOT, text=True)
                    for target in value.split():
                        selected.update(closure(target))
                self.assertNotIn('mutation-snapshot', selected)
        self.assertIn('refs', closure('test'))
        # Inspect reachable recipes too: no implicit execution/refresh added.
        for line in result.stdout.splitlines():
            if line.startswith('\t'):
                self.assertNotIn('tools/mutate.py', line)
                self.assertNotIn('mutsnap.py --update', line)
                self.assertNotIn('mutsnap.py --verify', line)


if __name__ == '__main__':
    unittest.main()
