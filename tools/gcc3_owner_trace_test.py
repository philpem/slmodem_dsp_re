#!/usr/bin/env python3
"""Real ELF controls for bounded owner tracing, including rejecting controls."""
from pathlib import Path
import subprocess
import sys
import tempfile
import threading

TOOLS = Path(__file__).resolve().parent
sys.path[:] = [p for p in sys.path if Path(p or '.').resolve() != TOOLS]
import unittest
from unittest import mock
sys.path.insert(0, str(TOOLS))
import gcc3_owner_trace as owner


class OwnerTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.serial = 0

    def obj(self, body):
        self.serial += 1
        source = self.root / ('c%d.s' % self.serial)
        path = source.with_suffix('.o')
        source.write_text('.text\n.globl f\n.type f,@function\nf:\n' + body + '\n.size f,.-f\n')
        subprocess.run(['as', '--32', '-o', str(path), str(source)],
                       check=True, capture_output=True)
        return path

    def compare(self, a, b):
        return owner.analyze(self.obj(a), self.obj(b), ['f'])

    def test_same_root_positive_with_copy_lea_and_spill(self):
        prefix = ('push %ebp\nmov %esp,%ebp\nsub $8,%esp\n'
                  'mov 8(%ebp),%eax\nlea 8(%eax),%esi\n'
                  'mov %esi,-4(%ebp)\nmov -4(%ebp),%edi\n')
        report = self.compare(prefix + 'movw $7,4(%edi)\nleave\nret',
                              'mov 4(%esp),%ecx\nmovl $7,12(%ecx)\nret')
        self.assertEqual(report['nomination_count'], 1)
        self.assertFalse(report['semantic_proof'])
        self.assertIn("('entry-arg', 1), 12", report['functions'][0]['nominations'][0]['machine_origin'])
        self.assertIn(str((TOOLS / 'gcc3_owner_trace.py').resolve()), report['sha256'])

    def test_distinct_entry_argument_roots_reject(self):
        report = self.compare('mov 4(%esp),%esi\nmovw $0,28(%esi)\nret',
                              'mov 8(%esp),%esi\nmovl $0,28(%esi)\nret')
        self.assertEqual(report['nomination_count'], 0)

    def test_pointer_load_and_call_return_are_local_provenance(self):
        for load in ('mov (%eax),%eax', 'call external'):
            report = self.compare('mov 4(%esp),%eax\n' + load + '\nmovw $1,4(%eax)\nret',
                                  'mov 4(%esp),%eax\n' + load + '\nmovl $1,4(%eax)\nret')
            self.assertEqual(report['nomination_count'], 0)
            self.assertEqual(report['functions'][0]['native']['stores'][-1]['status'], 'unknown')
            self.assertIn('pointer-load' if load.startswith('mov') else 'call-return',
                          repr(report['functions'][0]['native']['stores'][-1]['owner']))

    def test_partial_high_low_and_word_writes_destroy_pointer(self):
        for reg in ('al', 'ah', 'ax'):
            report = self.compare('mov 4(%esp),%eax\nmov $1,%' + reg + '\nmovw $7,4(%eax)\nret',
                                  'mov 4(%esp),%eax\nmovl $7,4(%eax)\nret')
            self.assertEqual(report['nomination_count'], 0)
            self.assertEqual(report['functions'][0]['native']['stores'][-1]['status'], 'unknown')
        self.assertEqual(owner.LANES['ah'], ('eax', 8, 8))
        self.assertEqual(owner.LANES['al'], ('eax', 8, 0))

    def test_high_and_low_lane_values_remain_distinct(self):
        report = self.compare('mov 4(%esp),%esi\nmov %al,4(%esi)\nret',
                              'mov 4(%esp),%esi\nmov %ah,4(%esi)\nret')
        f = report['functions'][0]
        self.assertEqual(f['native']['stores'][0]['value'][1], ('eax', 8, 0))
        self.assertEqual(f['candidate']['stores'][0]['value'][1], ('eax', 8, 8))
        self.assertEqual(report['nomination_count'], 0)

    def test_conflicting_branch_join_is_ambiguous(self):
        body = ('mov 4(%esp),%eax\ntest %ecx,%ecx\nje 1f\n'
                'mov 8(%esp),%eax\n1: movw $7,4(%eax)\nret')
        report = self.compare(body, body.replace('movw $7', 'movl $7'))
        self.assertEqual(report['nomination_count'], 0)
        self.assertEqual(report['functions'][0]['native']['denominator']['ambiguous'], 1)

    def test_equal_join_is_supported_and_loop_converges(self):
        body = ('mov 4(%esp),%eax\n1: test %ecx,%ecx\nje 2f\n'
                'mov %eax,%esi\njmp 1b\n2: movw $7,4(%eax)\nret')
        report = self.compare(body, body.replace('movw $7', 'movl $7'))
        self.assertEqual(report['nomination_count'], 1)

    def test_unsupported_instruction_invalidates_facts(self):
        body = 'mov 4(%esp),%eax\nxchg %eax,%ebx\nmovw $7,4(%eax)\nret'
        report = self.compare(body, body.replace('movw $7', 'movl $7'))
        native = report['functions'][0]['native']
        self.assertEqual(report['nomination_count'], 0)
        self.assertEqual(native['denominator']['unknown'], 1)
        self.assertEqual(len(native['unsupported_instruction_addresses']), 1)

    def test_call_clobbers_spills_and_preserves_register_copy(self):
        body = ('mov 4(%esp),%esi\nsub $4,%esp\nmov %esi,(%esp)\n'
                'call external\nmov (%esp),%eax\nmovw $7,4(%eax)\n'
                'movw $8,8(%esi)\nadd $4,%esp\nret')
        report = self.compare(body, body.replace('movw', 'movl'))
        self.assertEqual(report['nomination_count'], 1)
        self.assertEqual(report['functions'][0]['native']['denominator'],
                         {'supported': 1, 'unknown': 2, 'ambiguous': 0})

    def test_partial_spill_overwrite_invalidates_slot(self):
        body = ('mov 4(%esp),%eax\nsub $4,%esp\nmov %eax,(%esp)\n'
                'movb $1,1(%esp)\nmov (%esp),%esi\nmovw $7,4(%esi)\nret')
        report = self.compare(body, body.replace('movw $7', 'movl $7'))
        self.assertEqual(report['nomination_count'], 0)
        self.assertEqual(report['functions'][0]['native']['stores'][-1]['status'], 'unknown')

    def test_indirect_branch_refuses_whole_function(self):
        report = self.compare('mov 4(%esp),%eax\njmp *%eax',
                              'mov 4(%esp),%eax\njmp *%eax')
        self.assertEqual(report['functions'][0]['native']['status'], 'unknown')
        self.assertEqual(report['functions'][0]['native']['unclassified_instruction_count'], 2)

    def test_fallthrough_extent_refuses_whole_function(self):
        report = self.compare('mov 4(%esp),%eax\nmovw $7,4(%eax)',
                              'mov 4(%esp),%eax\nmovl $7,4(%eax)')
        self.assertEqual(report['functions'][0]['native']['status'], 'unknown')
        self.assertEqual(report['nomination_count'], 0)

    def test_same_width_cancellation_and_duplicate_ambiguity(self):
        body = 'mov 4(%esp),%eax\nmovw $7,4(%eax)\nret'
        self.assertEqual(self.compare(body, body)['nomination_count'], 0)
        duplicate = body.replace('ret', 'movw $7,4(%eax)\nret')
        self.assertEqual(self.compare(duplicate, duplicate.replace('movw', 'movl'))['nomination_count'], 0)

    def test_changed_loaded_dependency_refuses_before_analysis(self):
        path = self.obj('ret')
        dependency = str((TOOLS / 'gcc3_search_width.py').resolve())
        original = owner.width.sha256
        def changed(p):
            return 'changed' if str(p) == dependency else original(p)
        with mock.patch.object(owner.width, 'sha256', side_effect=changed):
            with self.assertRaisesRegex(ValueError, 'changed since import'):
                owner.analyze(path, path, ['f'])

    def test_write_and_restore_uses_captured_elf_bytes(self):
        native = self.obj('mov 4(%esp),%eax\nmovw $7,4(%eax)\nret')
        candidate = self.obj('mov 8(%esp),%eax\nmovl $7,4(%eax)\nret')
        transient = self.obj('mov 4(%esp),%eax\nmovl $7,4(%eax)\nret')
        original = candidate.read_bytes()
        original_hash = owner.width.sha256(candidate)
        # Known negative control: the transient ELF would nominate a width
        # change, whereas the captured ELF has a distinct argument owner.
        self.assertEqual(owner.analyze(native, candidate, ['f'])['nomination_count'], 0)
        self.assertEqual(owner.analyze(native, transient, ['f'])['nomination_count'], 1)
        swap, swapped, restore, restored = (threading.Event() for _ in range(4))
        errors, observed = [], []

        def writer():
            try:
                if not swap.wait(10):
                    raise RuntimeError('trace did not request transient ELF')
                candidate.write_bytes(transient.read_bytes())
                swapped.set()
                if not restore.wait(10):
                    raise RuntimeError('trace did not release transient ELF')
            except BaseException as exc:
                errors.append(exc)
            finally:
                candidate.write_bytes(original)
                restored.set()

        real_trace = owner.trace

        def scheduled_trace(path, symbol):
            observed.append((Path(path), Path(path).stat().st_mode))
            if len(observed) != 2:
                return real_trace(path, symbol)
            swap.set()
            self.assertTrue(swapped.wait(10), 'writer failed to supply transient ELF')
            try:
                return real_trace(path, symbol)
            finally:
                restore.set()
                self.assertTrue(restored.wait(10), 'writer failed to restore ELF')

        thread = threading.Thread(target=writer, daemon=True)
        thread.start()
        try:
            with mock.patch.object(owner, 'trace', side_effect=scheduled_trace):
                report = owner.analyze(native, candidate, ['f'])
        finally:
            swap.set()
            restore.set()
            thread.join(10)
        self.assertFalse(thread.is_alive())
        self.assertEqual(errors, [])
        self.assertEqual(report['nomination_count'], 0)
        self.assertEqual(report['inputs'], {'native': str(native), 'candidate': str(candidate)})
        self.assertEqual(report['sha256'][str(candidate)], original_hash)
        self.assertEqual(candidate.read_bytes(), original)
        self.assertEqual(len(observed), 2)
        for path, mode in observed:
            self.assertNotIn(path, (native, candidate))
            self.assertEqual(mode & 0o222, 0)
            self.assertFalse(path.exists(), 'session snapshot was not cleaned up')

    def test_persistent_input_change_still_refuses(self):
        native = self.obj('ret')
        candidate = self.obj('ret')
        real_trace = owner.trace

        def changed(path, symbol):
            report = real_trace(path, symbol)
            candidate.write_bytes(b'changed')
            return report

        with mock.patch.object(owner, 'trace', side_effect=changed):
            with self.assertRaisesRegex(ValueError, 'input/dependency changed during analysis'):
                owner.analyze(native, candidate, ['f'])

    def test_cli_denominators_and_no_overwrite(self):
        a = self.obj('mov 4(%esp),%eax\nmovw $7,4(%eax)\nret')
        b = self.obj('mov 4(%esp),%eax\nmovl $7,4(%eax)\nret')
        output = self.root / 'report.json'
        args = [sys.executable, str(TOOLS / 'gcc3_owner_trace.py'), '--native', str(a),
                '--candidate', str(b), '--symbol', 'f', '--output', str(output)]
        run = subprocess.run(args, check=True, capture_output=True, text=True)
        self.assertIn('supported', run.stdout)
        saved = output.read_bytes()
        self.assertNotEqual(subprocess.run(args, capture_output=True).returncode, 0)
        self.assertEqual(output.read_bytes(), saved)


if __name__ == '__main__':
    unittest.main()
