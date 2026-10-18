#!/usr/bin/env python3
"""Multi-file mutation plumbing controls; no compiler or real mutants."""
import contextlib
import io
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
import mutation_source as source
import mutate
import anchorcheck
import refcheck


class SourceControls(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        (self.root / 'include').mkdir()
        self.facade = self.root / 'source.c'
        self.facade.write_text('void\nowner(void)\n{\n#include "one.inc"\n}\n'
                               'void\nother(void)\n{\n#include "two.inc"\n}\n')
        (self.root / 'include/one.inc').write_text('first = 1;\n')
        (self.root / 'include/two.inc').write_text('second = 2;\n')
        self.row = dict(label='first', source='include/one.inc', fn='owner',
                        find='first = 1;', replace='first = 3;')

    def resolve(self, row=None):
        return source.resolve_source('source.c', row or self.row, self.root)

    def test_valid_and_default(self):
        self.assertEqual(self.resolve(), (Path('include/one.inc'), 'owner'))
        self.assertEqual(self.resolve({'find':'anything'}), (Path('source.c'), None))

    def test_wrong_owner(self):
        with self.assertRaisesRegex(source.SourceError, 'literal include'):
            self.resolve(dict(self.row, fn='other'))

    def test_missing_owner(self):
        row = dict(self.row);del row['fn']
        with self.assertRaisesRegex(source.SourceError, 'explicit fn'):
            self.resolve(row)

    def test_missing_include(self):
        self.facade.write_text('void\nowner(void)\n{\n}\n')
        with self.assertRaisesRegex(source.SourceError, 'literal include'):
            self.resolve()

    def test_commented_include_and_neighbor_not_proof(self):
        self.facade.write_text('void\nowner(void)\n{\n/*\n#include "one.inc"\n*/\n}\n'
                               '#include "one.inc"\n')
        with self.assertRaisesRegex(source.SourceError, 'literal include'):
            self.resolve()

    def test_disabled_or_conditional_include(self):
        for condition in ('0', '1', 'UNKNOWN'):
            self.facade.write_text('void\nowner(void)\n{\n#if '+condition+
                                   '\n#include "one.inc"\n#endif\n}\n')
            with self.subTest(condition=condition), self.assertRaises(source.SourceError):
                self.resolve()

    def test_conditional_function_is_not_owner_proof(self):
        self.facade.write_text('#if 0\n'+self.facade.read_text()+'#endif\n')
        with self.assertRaises(source.SourceError):self.resolve()

    def test_header_guard_allowed_but_nested_condition_rejected(self):
        original = self.facade.read_text()
        self.facade.write_text('#ifndef TEST_GUARD\n#define TEST_GUARD\n'+original+'#endif\n')
        self.resolve()
        self.facade.write_text('#ifndef TEST_GUARD\n#define TEST_GUARD\n#if 0\n'+original+'#endif\n#endif\n')
        with self.assertRaises(source.SourceError):self.resolve()

    def test_adjacent_include_shadows_include_directory(self):
        (self.root/'one.inc').write_text('different adjacent include')
        with self.assertRaises(source.SourceError):self.resolve()

    def test_duplicate(self):
        (self.root / 'include/one.inc').write_text('first = 1; first = 1;')
        with self.assertRaisesRegex(source.SourceError, 'exactly once'):
            self.resolve()

    def test_vacuous(self):
        with self.assertRaisesRegex(source.SourceError, 'vacuous'):
            self.resolve(dict(self.row, replace=self.row['find']))

    def test_path_escape(self):
        for path in ('../escape', str(self.root / 'include/one.inc')):
            with self.subTest(path=path), self.assertRaises(source.SourceError):
                self.resolve(dict(self.row, source=path))

    def test_symlink_escape(self):
        (self.root / 'include/escape.inc').symlink_to('/etc/passwd')
        with self.assertRaisesRegex(source.SourceError, 'outside'):
            self.resolve(dict(self.row, source='include/escape.inc'))


    def test_anchorcheck_preserves_microstate_ownership(self):
        (self.root / 'test/mutations').mkdir(parents=True)
        (self.root / 'test/mutations/suites.json').write_text(
            json.dumps({'demo':['source.c','build/test/mock']}))
        self.facade.write_text(self.facade.read_text() +
            'void\ndispatch(void)\n{\nswitch (state) {\n'
            'case V34HS_ONE:\n owner();\n'
            'case V34HS_TWO:\n other();\n}\n}\n')
        previous = Path.cwd()
        try:
            os.chdir(self.root)
            for label, expected in [('47 correct',0), ('48 wrong owner',1)]:
                (self.root/'test/mutations/demo.json').write_text(
                    json.dumps([dict(self.row, label=label)]))
                with patch.object(sys,'argv',['anchorcheck.py','demo']), \
                     patch.object(anchorcheck,'state_values',return_value={'V34HS_ONE':47,'V34HS_TWO':48}), \
                     contextlib.redirect_stdout(io.StringIO()):
                    self.assertEqual(anchorcheck.main(), expected)
        finally:
            os.chdir(previous)

    def check_live(self, row=None):
        (self.root/'test/mutations').mkdir(parents=True, exist_ok=True)
        (self.root/'test/mutations/suites.json').write_text(
            json.dumps({'demo':['source.c','build/test/mock']}))
        (self.root/'test/mutations/demo.json').write_text(json.dumps([row or self.row]))
        previous = Path.cwd()
        output = io.StringIO()
        try:
            os.chdir(self.root)
            with contextlib.redirect_stdout(output):
                result = refcheck.check_live_mutants()
        finally:
            os.chdir(previous)
        return result, output.getvalue()

    def test_live_detector_clean_include(self):
        result, text = self.check_live()
        self.assertEqual(result, [])
        self.assertIn('1 suite(s), 1 target(s), 0 live or invalid', text)

    def test_live_detector_applied_include_mutation(self):
        (self.root/'include/one.inc').write_text('first = 3;\n')
        result, text = self.check_live()
        self.assertEqual(result, ['include/one.inc'])
        self.assertIn('LIVE MUTANT', text)
        # Relaxed presence is exclusive to inspection; mutation/anchor checks
        # still reject the now-absent original anchor.
        with self.assertRaisesRegex(source.SourceError, 'exactly once'):self.resolve()

    def test_live_detector_never_relaxes_owner(self):
        result, text = self.check_live(dict(self.row, fn='other'))
        self.assertEqual(result, ['include/one.inc'])
        self.assertIn('BAD MUTATION SOURCE', text)

    def test_live_detector_never_relaxes_path(self):
        result, text = self.check_live(dict(self.row, source='../escape'))
        self.assertEqual(result, ['../escape'])
        self.assertIn('BAD MUTATION SOURCE', text)

    def test_live_detector_default_source_unchanged(self):
        row = dict(label='default', find='owner(void)', replace='changed(void)')
        self.assertEqual(self.check_live(row)[0], [])
        self.facade.write_text(self.facade.read_text().replace(row['find'],row['replace']))
        result, text = self.check_live(row)
        self.assertEqual(result, ['source.c'])
        self.assertIn('LIVE MUTANT', text)

    def run_mocked_runner(self, rc=1, throws=False, only=None, shard=None):
        rows = [self.row, dict(label='second', source='include/two.inc', fn='other',
                              find='second = 2;', replace='second = 4;')]
        (self.root / 'rows.json').write_text(json.dumps(rows))
        argv = ['mutate.py','source.c','build/test/mock','rows.json']
        if only is not None:argv += ['--only', only]
        if shard is not None:argv += ['--shard', shard]
        seen = []
        def build(*args):
            if len(args) == 3:return 0, '', 'test'
            first = (self.root / 'include/one.inc').read_text()
            second = (self.root / 'include/two.inc').read_text()
            # Exactly one mutant is active; earlier targets must be restored.
            self.assertNotEqual('first = 3;' in first, 'second = 4;' in second)
            seen.append('first' if 'first = 3;' in first else 'second')
            if throws:raise RuntimeError('mock build exception')
            return rc, '', 'test'
        previous = Path.cwd()
        try:
            os.chdir(self.root)
            with patch.object(sys,'argv',argv), patch.object(mutate,'preflight'), \
                 patch.object(mutate,'tree_relative',side_effect=lambda p, what:p), \
                 patch.object(mutate,'enter_workdir'), patch.object(mutate,'take_lock'), \
                 patch.object(mutate,'build_and_run',side_effect=build), \
                 contextlib.redirect_stdout(io.StringIO()):
                if throws:
                    with self.assertRaisesRegex(RuntimeError, 'mock build exception'):mutate.main()
                else:mutate.main()
        finally:
            os.chdir(previous)
        self.assertEqual((self.root/'include/one.inc').read_text(), 'first = 1;\n')
        self.assertEqual((self.root/'include/two.inc').read_text(), 'second = 2;\n')
        return seen

    def test_runner_restores_two_files_caught(self):
        self.assertEqual(self.run_mocked_runner(), ['first','second'])

    def test_runner_restores_survivors(self):
        self.assertEqual(self.run_mocked_runner(rc=0), ['first','second'])

    def test_runner_restores_compile_failure(self):
        self.assertEqual(self.run_mocked_runner(rc=None), ['first','second'])

    def test_runner_restores_exception(self):
        self.assertEqual(self.run_mocked_runner(throws=True), ['first'])

    def test_selection_and_shard_stable(self):
        self.assertEqual(self.run_mocked_runner(only='second'), ['second'])
        self.assertEqual(self.run_mocked_runner(shard='1/2'), ['second'])


if __name__ == '__main__':
    unittest.main()
