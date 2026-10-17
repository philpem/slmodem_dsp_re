#!/usr/bin/env python3
"""Small mocked runner controls: no builds, copies, or mutation-suite cycles."""
import contextlib
import importlib.util
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]


def module(name):
    spec = importlib.util.spec_from_file_location(name, ROOT/'tools'/(name+'.py'))
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


mutate = module('mutate')
mutsnap = module('mutsnap')
ENTRIES = [{'note':'not a mutation'}] + [
    {'label':('selected' if i < 4 else 'other')+'-%d'%i,'find':'x','replace':'y'}
    for i in range(12)]


class ParallelSubset(unittest.TestCase):
    def run_workers(self, only, verbose=False, failed=False, uncaught=False):
        commands, executed = [], []

        def worker(command, **kwargs):
            commands.append(command)
            needle = command[command.index('--only')+1] if '--only' in command else None
            selected = [m for m in ENTRIES if 'find' in m and
                        (needle is None or needle in m['label'])]
            shard = command[command.index('--shard')+1]
            rows = mutate.shard_of(selected, shard)
            executed.extend(m['label'] for m in rows)
            text = ''.join('  ok    %-52s  caught (test)\n'%m['label'] for m in rows)
            if '--verbose' in command:
                text += '      failing check: known mock assertion\n'
            data = {'text':text,'total':len(rows),
                    'uncaught':[rows[0]['label']] if uncaught and rows else [],
                    'broken':[],'equivalent':[],'surprises':[],
                    'by':{'test':len(rows),'strings':0,'hang':0}}
            output = 'worker died' if failed else mutate.SHARD_MARK+json.dumps(data)+'\n'
            return SimpleNamespace(communicate=lambda:(output,None))

        args = SimpleNamespace(suite='demo', source=None, test=None, mutations=None,
                               only=only, verbose=verbose)
        out = io.StringIO()
        with patch.object(mutate.subprocess,'Popen',side_effect=worker), contextlib.redirect_stdout(out):
            rc = mutate.run_parallel(args, 2, 12)
        return rc,out.getvalue(),commands,executed

    def test_parallel_filter_verbose_and_nonrecordable_parent(self):
        rc,text,commands,executed = self.run_workers('selected', True)
        self.assertEqual(rc,0)
        self.assertEqual(executed,['selected-%d'%i for i in range(4)])
        self.assertEqual(len(commands),2)
        self.assertTrue(all('--only' in c and '--verbose' in c for c in commands))
        self.assertIn('failing check: known mock assertion',text)
        self.assertIn('SUBSET: 4 of 12',text)
        self.assertNotIn('mutations:',text)
        self.assertIn('cannot be a baseline',text)

    def test_unfiltered_parallel_still_reports_full_suite(self):
        rc,text,commands,executed = self.run_workers(None)
        self.assertEqual(rc,0)
        self.assertEqual(len(executed),12)
        self.assertTrue(all('--only' not in c and '--verbose' not in c for c in commands))
        self.assertIn('12 mutations:',text)
        self.assertNotIn('SUBSET:',text)

    def test_empty_filter_is_explicit_nonrecordable_subset(self):
        rc,text,commands,executed = self.run_workers('')
        self.assertEqual(rc,0)
        self.assertEqual(len(executed),12)
        self.assertIn('SUBSET: 12 of 12',text)
        self.assertNotIn('mutations:',text)

    def test_uncaught_subset_fails_without_full_summary(self):
        rc,text,commands,executed = self.run_workers('selected',uncaught=True)
        self.assertEqual(rc,1)
        self.assertIn('SUBSET:',text)
        self.assertNotIn('mutations:',text)

    def test_dead_worker_is_not_a_subset_result(self):
        rc,text,commands,executed = self.run_workers('selected',failed=True)
        self.assertEqual(rc,2)
        self.assertIn('this run is not a result',text)
        self.assertNotIn('mutations:',text)
        self.assertNotIn('SUBSET:',text)

    def test_worker_budget_counts_selected_mutations_only(self):
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)/'mutations.json';path.write_text(json.dumps(ENTRIES))
            argv=['mutate.py','src.c','build/test/demo',str(path),'--jobs','8','--only','selected']
            with patch.object(sys,'argv',argv), patch.object(mutate,'preflight'), \
                 patch.object(mutate,'run_parallel',return_value=19) as run:
                self.assertEqual(mutate.main(),19)
            self.assertEqual(run.call_args.args[1:],(2,12))

    def test_one_selected_mutation_does_not_spawn_workers(self):
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)/'mutations.json';path.write_text(json.dumps(ENTRIES))
            argv=['mutate.py','src.c','build/test/demo',str(path),'--jobs','8','--only','selected-0']
            with patch.object(sys,'argv',argv), patch.object(mutate,'preflight'), \
                 patch.object(mutate,'tree_relative',side_effect=lambda p,what:p), \
                 patch.object(mutate,'enter_workdir',side_effect=RuntimeError('serial leaf reached')), \
                 patch.object(mutate,'run_parallel') as run:
                with self.assertRaisesRegex(RuntimeError,'serial leaf reached'):
                    mutate.main()
                run.assert_not_called()

    def test_all_with_filter_is_rejected_before_dispatch(self):
        argv=['mutate.py','--all','--only','selected','--jobs','2']
        with patch.object(sys,'argv',argv), patch.object(mutate,'run_all') as run, \
             patch.object(mutate,'preflight') as preflight, contextlib.redirect_stderr(io.StringIO()):
            with self.assertRaises(SystemExit) as error:
                mutate.main()
            self.assertEqual(error.exception.code,2)
            run.assert_not_called();preflight.assert_not_called()

    def test_no_match_stops_before_any_worker_or_copy(self):
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)/'mutations.json';path.write_text(json.dumps(ENTRIES))
            argv=['mutate.py','src.c','build/test/demo',str(path),'--jobs','8','--only','absent']
            with patch.object(sys,'argv',argv), patch.object(mutate,'preflight'), \
                 patch.object(mutate,'enter_workdir') as copy, patch.object(mutate,'run_parallel') as run:
                with self.assertRaisesRegex(SystemExit,'no mutation label contains'):
                    mutate.main()
                run.assert_not_called();copy.assert_not_called()


class SnapshotSubset(unittest.TestCase):
    verdict='  ok    %-52s  caught (test)\n'%'selected-0'

    def parse(self,text):
        with patch.object(mutsnap.subprocess,'run',return_value=SimpleNamespace(stdout=text)):
            return mutsnap.run_suite('demo',2)

    def test_subset_with_verdict_lines_is_not_recordable(self):
        verdicts,summary,out=self.parse(self.verdict+'  SUBSET: 1 of 12 mutations\n')
        self.assertEqual(verdicts,{})
        self.assertIn('FAILED',summary)
        self.assertIn('SUBSET',summary)

    def test_subset_marker_overrides_accidental_full_summary(self):
        verdicts,summary,out=self.parse(self.verdict+'  1 mutations: 1 caught\n  SUBSET: 1 of 12 mutations\n')
        self.assertEqual(verdicts,{})
        self.assertIn('FAILED',summary)

    def test_partial_verdicts_without_summary_are_not_recordable(self):
        verdicts,summary,out=self.parse(self.verdict+'  SHARD 1 DIED\n')
        self.assertEqual(verdicts,{})
        self.assertEqual(summary,'FAILED')

    def test_full_summary_still_parses(self):
        verdicts,summary,out=self.parse(self.verdict+'  1 mutations: 1 caught\n')
        self.assertEqual(verdicts,{'selected-0':'caught'})
        self.assertEqual(summary,'1 mutations: 1 caught')

    def test_snapshot_update_preserves_existing_entry_on_subset(self):
        text=self.verdict+'  SUBSET: 1 of 12 mutations\n'
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)/'snapshot.json'
            old={'key':'old-key','summary':'old result','verdicts':{'old':'caught'}}
            path.write_text(json.dumps({'suites':{'demo':old}}))
            with patch.object(mutsnap,'SNAP',str(path)), \
                 patch.object(mutsnap,'registered',return_value={'demo':['source','binary']}), \
                 patch.object(mutsnap,'declared_entry',return_value=None), \
                 patch.object(mutsnap,'suite_key',side_effect=AssertionError('subset must not get a key')), \
                 patch.object(mutsnap.subprocess,'run',return_value=SimpleNamespace(stdout=text)), \
                 contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(mutsnap.cmd_update(SimpleNamespace(suite=['demo'],jobs=2)),2)
            self.assertEqual(json.loads(path.read_text())['suites']['demo'],old)


if __name__=='__main__':
    unittest.main(verbosity=2)
