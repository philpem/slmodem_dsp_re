#!/usr/bin/env python3
"""Real ELF positive/negative controls for the variant report; no compiler claims."""
from pathlib import Path
import shutil
import json
import subprocess
import sys
import tempfile
import threading
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

    def test_rewrite_then_restore_cannot_substitute_unhashed_bytes(self):
        from unittest import mock
        base = self.baseline()
        other = self.obj('other', [('f', 'mov $2,%eax\nmov %eax,(%edx)\nret'), ('g', 'ret')])
        candidate = self.root / 'candidate.o'
        original = base.read_bytes()
        candidate.write_bytes(original)
        session = report.ComparisonSession(base)
        ready, switched, restore, restored = [threading.Event() for _ in range(4)]
        errors = []
        def writer():
            try:
                if not ready.wait(10):
                    raise RuntimeError('writer not scheduled')
                candidate.write_bytes(other.read_bytes())
                switched.set()
                if not restore.wait(10):
                    raise RuntimeError('restore not scheduled')
            except Exception as exc:
                errors.append(str(exc))
            finally:
                candidate.write_bytes(original)
                restored.set()
        real_compare = report._compare
        def during_analysis(*args, **kwargs):
            ready.set()
            self.assertTrue(switched.wait(10))
            try:
                return real_compare(*args, **kwargs)
            finally:
                restore.set()
                self.assertTrue(restored.wait(10))
        thread = threading.Thread(target=writer)
        thread.start()
        try:
            with mock.patch.object(report, '_compare', during_analysis):
                result = session.compare(base, candidate)
        finally:
            ready.set()
            restore.set()
            thread.join(10)
        self.assertFalse(thread.is_alive())
        self.assertEqual(errors, [])
        self.assertEqual(result['functions']['f']['candidate']['raw'][0], 'EXACT')
        self.assertEqual(session.compare(base, candidate)['functions'],
                         report.ComparisonSession(base).compare(base, candidate)['functions'])

    def relocation_obj(self, name, immediate=1, target='f', target_offset=0):
        source = self.root / (name + '.s')
        obj = self.root / (name + '.o')
        text = ('.text\n'
                '.globl f\n.type f,@function\nf:\nmov $%d,%%eax\nret\n.size f,.-f\n' % immediate +
                '.globl g\n.type g,@function\ng:\nmov $2,%eax\nret\n.size g,.-g\n'
                '.data\n.long ' + target + ('+%d' % target_offset) + '\n')
        source.write_text(text)
        subprocess.run(['as', '--32', '-o', str(obj), str(source)], check=True, capture_output=True)
        return obj

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

    def test_cli_batch_writes_all_reports_without_overwrite(self):
        base = self.baseline()
        same = self.obj('same', [('f', 'mov $1,%eax\nmov %eax,(%edx)\nret'), ('g', 'ret')])
        extra = self.obj('extra', [('f', 'ret')])
        outdir = self.root / 'batch'
        args = ['--baseline', str(base), '--blob', str(base),
                '--candidate', str(same), '--candidate', str(extra), '--output-dir', str(outdir)]
        self.assertEqual(report.main(args), 0)
        paths = sorted(outdir.glob('*.json'))
        self.assertEqual(len(paths), 2)
        rows = [json.loads(path.read_text()) for path in paths]
        self.assertEqual([row['shared_sized_functions'] for row in rows], [2, 1])
        before = {path.name: path.read_text() for path in paths}
        self.assertEqual(report.main(args), 1)
        self.assertEqual({path.name: path.read_text() for path in paths}, before)

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

    def test_session_and_batch_match_fresh_reports_and_reuse_caches(self):
        base = self.baseline()
        variant = self.obj('variant', [('f', 'mov $1,%ecx\nmov %ecx,(%edx)\nret'), ('g', 'ret')])
        fresh = report.compare(base, variant, base)
        session = report.ComparisonSession(base)
        first = session.compare(base, variant)
        before = session.cache_info()['byteident']['body']['hits']
        second = session.compare(base, variant)
        after = session.cache_info()['byteident']['body']['hits']
        self.assertEqual(first, fresh)
        self.assertEqual(second, fresh)
        self.assertEqual(session.cache_info()['comparisons'], 2)
        self.assertGreater(after, before)
        self.assertEqual(session.cache_info()['size_inventory_misses'], 2)
        batch = report.compare_many([(base, variant), (base, variant)], base)
        self.assertEqual(batch, [fresh, fresh])

    def test_same_path_rewrite_invalidates_code_and_data_relocation_caches(self):
        base = self.relocation_obj('base-reloc', immediate=1, target='f')
        candidate = self.root / 'candidate-reloc.o'
        shutil.copyfile(base, candidate)
        session = report.ComparisonSession(base)
        session._jumptable.section_direct_edges(str(candidate.resolve()), '.text')
        self.assertGreater(session.cache_info()['jumptable']['section_direct_edges']['currsize'], 0)
        before = session.compare(base, candidate)
        before_targets = before['allocated_data_relocation_audit']['code_targets']
        self.assertGreater(before_targets['compared'], 0)
        self.assertEqual(before_targets['changed'], 0)
        self.assertGreater(session.cache_info()['jumptable']['instructions']['currsize'], 0)

        rewritten = self.relocation_obj('rewritten-reloc', immediate=3, target='g', target_offset=5)
        shutil.copyfile(rewritten, candidate)
        other_session = report.ComparisonSession(base)
        cached_other = other_session.compare(base, candidate)
        fresh = report.compare(base, candidate, base)
        fresh_path = self.root / 'fresh-process.json'
        subprocess.run([sys.executable, str(TOOLS / 'gcc3_search_compare.py'),
                        '--baseline', str(base), '--candidate', str(candidate),
                        '--blob', str(base), '--output', str(fresh_path)],
                       check=True, capture_output=True, text=True)
        fresh_process = json.loads(fresh_path.read_text())
        cached = session.compare(base, candidate)
        self.assertEqual(cached_other, fresh)
        self.assertEqual(fresh_process, fresh)
        cached_again = session.compare(base, candidate)
        self.assertEqual(cached, fresh)
        self.assertEqual(cached_again, fresh)
        self.assertEqual(cached['functions']['f']['candidate']['grade'][0], 'BYTES')
        self.assertEqual(cached['allocated_data_relocation_audit']['code_targets']['changed'], 1)
        self.assertFalse(cached['allocated_data_raw_unchanged'])
        self.assertEqual(session.cache_info()['path_content_invalidations'], 1)
        self.assertEqual(session.cache_info()['jumptable']['section_direct_edges']['currsize'], 0)

        shutil.copyfile(base, candidate)
        restored = session.compare(base, candidate)
        fresh_restored = report.compare(base, candidate, base)
        self.assertEqual(restored, fresh_restored)
        self.assertEqual(restored['functions']['f']['candidate']['grade'][0], 'EXACT')
        self.assertEqual(restored['allocated_data_relocation_audit']['code_targets']['changed'], 0)
        self.assertEqual(session.cache_info()['path_content_invalidations'], 2)

    def test_session_refuses_input_mutation_during_comparison(self):
        base = self.baseline()
        candidate = self.obj('candidate', [('f', 'ret')])
        session = report.ComparisonSession(base)
        original_bytes = candidate.read_bytes()
        original = report._compare

        def rewrite_after_analysis(*args, **kwargs):
            result = original(*args, **kwargs)
            candidate.write_bytes(candidate.read_bytes() + b'changed')
            return result

        from unittest import mock
        with mock.patch.object(report, '_compare', side_effect=rewrite_after_analysis):
            with self.assertRaisesRegex(ValueError, 'object changed during comparison'):
                session.compare(base, candidate)
        self.assertEqual(session.cache_info()['path_content_invalidations'], 1)
        candidate.write_bytes(original_bytes)
        reused = session.compare(base, candidate)
        fresh = report.compare(base, candidate, base)
        self.assertEqual(reused, fresh)

    def test_session_invalidates_when_input_is_deleted_during_comparison(self):
        base = self.baseline()
        candidate = self.obj('candidate', [('f', 'ret')])
        original_bytes = candidate.read_bytes()
        session = report.ComparisonSession(base)
        original = report._compare

        def delete_after_analysis(*args, **kwargs):
            result = original(*args, **kwargs)
            candidate.unlink()
            return result

        from unittest import mock
        with mock.patch.object(report, '_compare', side_effect=delete_after_analysis):
            with self.assertRaisesRegex(ValueError, 'object changed during comparison'):
                session.compare(base, candidate)
        self.assertEqual(session.cache_info()['path_content_invalidations'], 1)
        candidate.write_bytes(original_bytes)
        reused = session.compare(base, candidate)
        fresh = report.compare(base, candidate, base)
        self.assertEqual(reused, fresh)


if __name__ == '__main__':
    unittest.main()
