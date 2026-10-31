#!/usr/bin/env python3
"""Firing controls for diagnostic runs and conservative defining-copy coverage."""
import contextlib
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

import byteident as b
import fidelity as f


REFERENCE = [('mov', '$0x1,%eax'), ('add', '0x4(%ebx),%eax'),
             ('xor', '%ecx,%eax'), ('ret', '')]


class RunControls(unittest.TestCase):
    def measure(self, candidate, reference=REFERENCE):
        return f.measure(reference, candidate, b._padding)

    def test_register_renaming_and_parser_padding(self):
        candidate = [('nop', ''), ('mov', '$0x1,%edx'), ('add', '0x4(%esi),%edx'),
                     ('xor', '%ebx,%edx'), ('ret', '')]
        self.assertEqual(self.measure(candidate), dict(reference_instructions=4,
                         candidate_instructions=4, longest_run=4, reference_percent=100.0))

    def test_changed_operand_controls_fire(self):
        controls = [
            ('immediate', ('mov', '$0x1,%eax'), ('mov', '$0x2,%eax')),
            ('field offset', ('mov', '0x4(%eax),%ecx'), ('mov', '0x8(%eax),%ecx')),
            ('scale', ('mov', '(%eax,%edx,2),%ecx'), ('mov', '(%eax,%edx,4),%ecx')),
            ('register width', ('mov', '%ax,(%ebx)'), ('mov', '%eax,(%ebx)')),
            ('byte lane', ('mov', '%al,(%ebx)'), ('mov', '%ah,(%ebx)')),
            ('x87 index', ('fadd', '%st(1),%st'), ('fadd', '%st(2),%st')),
            ('condition', ('je', '.+8'), ('jne', '.+8')),
            ('indirect call', ('call', '*%eax'), ('call', '*0x4(%eax)')),
            ('callee', ('call', ".+1 @R_386_PC32:('symbol', 'foo', 0)"),
                       ('call', ".+1 @R_386_PC32:('symbol', 'bar', 0)")),
            ('symbol addend', ('call', ".+1 @R_386_PC32:('symbol', 'foo', 0)"),
                             ('call', ".+1 @R_386_PC32:('symbol', 'foo', 4)")),
            ('relocation kind', ('mov', "@R_386_32:('symbol', 'foo', 0),%eax"),
                                ('mov', "@R_386_PC32:('symbol', 'foo', 0),%eax")),
            ('section', ('mov', "@R_386_32:('section', '.data', 0),%eax"),
                        ('mov', "@R_386_32:('section', '.rodata', 0),%eax")),
        ]
        for label, reference, candidate in controls:
            with self.subTest(control=label):
                self.assertEqual(self.measure([candidate], [reference])['longest_run'], 0)
                self.assertEqual(self.measure([reference], [reference])['longest_run'], 1)

    def test_only_layout_contexts_are_abstracted(self):
        self.assertEqual(f.canonical(('je', '.+8')), f.canonical(('je', '.+128')))
        self.assertEqual(f.canonical(('mov', "@R_386_32:('section', '.rodata', 4),%eax")),
                         f.canonical(('mov', "@R_386_32:('section', '.rodata', 40),%edx")))
        self.assertNotEqual(f.canonical(('mov', '$0x4,%eax')), f.canonical(('mov', '$0x28,%eax')))
        self.assertNotEqual(f.canonical(('mov', '0x4(%eax),%edx')),
                            f.canonical(('mov', '0x28(%eax),%edx')))
        self.assertNotEqual(f.canonical(('mov', '(%esp),%eax')), f.canonical(('mov', '(%eax),%edx')))

    def test_insertion_splits_run_not_lcs(self):
        result = self.measure(REFERENCE[:2] + [('inc', '%eax')] + REFERENCE[2:])
        self.assertEqual(result['longest_run'], 2)
        self.assertEqual(result['candidate_instructions'], 5)

    def test_full_reference_run_does_not_certify_extra_instructions(self):
        self.assertEqual(self.measure(REFERENCE + [('inc', '%eax')])['reference_percent'], 100.0)
        self.assertEqual(f.category('SIZE'), 'NONMATCH')
        self.assertEqual(f.category('BYTES'), 'NONMATCH')
        self.assertEqual(f.category('RELOC'), 'NONMATCH')
        self.assertEqual(f.category('UNRESOLVED'), 'UNRESOLVED')

    def test_empty_stream_is_unavailable_not_a_clean_zero(self):
        with self.assertRaisesRegex(ValueError, 'empty'):
            self.measure([])
        with self.assertRaisesRegex(ValueError, 'empty'):
            self.measure(REFERENCE, [])

    def collect(self, candidates):
        scored = [(('SIZE' if name == 'size.o' else 'BYTES', 1), name)
                  for name in candidates]
        def read(path, symbol):
            return REFERENCE if path == b.BLOB else candidates[path]
        return b.function_fidelity('example', scored,
                                   {None: 12, **{name: 13 for name in candidates}}, read, 'SIZE')

    def test_worst_run_is_independent_of_legacy_size_bucket(self):
        row = self.collect({'size.o': REFERENCE + [('inc', '%eax')],
                            'bytes.o': [('mov', '$0x2,%eax')] + REFERENCE[1:]})
        self.assertEqual(row['object'], 'bytes.o')
        self.assertEqual(row['instructions']['longest_run'], 3)
        self.assertEqual(row['defining_copies'], 2)

    def test_unmeasured_copy_cannot_be_hidden_by_measured_copy(self):
        row = self.collect({'size.o': REFERENCE, 'empty.o': []})
        self.assertEqual(row['object'], 'empty.o')
        self.assertIsNone(row['instructions'])
        self.assertIn('empty', row['run_unavailable'])
        summary = f.summary({'example': row})
        self.assertEqual((summary['measured_functions'], summary['unavailable_functions']), (0, 1))
        lines = []
        f.print_rows({'example': row}, 5, lines.append)
        self.assertIn('0 measured, 1 unavailable; showing 0', '\n'.join(lines))

    def test_ranking_uses_run_coverage_not_byte_delta(self):
        def row(candidate, delta):
            return dict(instructions=self.measure(candidate), fidelity='NONMATCH',
                        reference_bytes=12, candidate_bytes=12 + delta, byte_delta=delta)
        rows = {'same-size-wrong': row([('inc', '%eax')], 0),
                'longer-matching': row(REFERENCE + [('inc', '%eax')], 10)}
        lines = []
        f.print_rows(rows, 1, lines.append)
        self.assertIn('longer-matching', lines[-1])
        self.assertNotIn('same-size-wrong', lines[-1])
        self.assertIn('4 / 4 (100.0%)', lines[-1])


    def test_rejected_bodies_remain_nodata_without_disassembly(self):
        self.assertEqual(b.byte_extent(None, 12), 12)
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "metrics.json"
            for args in (["--why", "example"], ["--json-out", str(output), "--limit", "0"]):
                with self.subTest(command=args), patch.object(b.glob, "glob", return_value=["copy.o"]), \
                     patch.object(b, "sizes", return_value={"example": 12}), \
                     patch.object(b, "body", return_value=(None, None)), \
                     patch.object(b, "_staleness", return_value=(None, None)), \
                     patch.object(b, "insns", side_effect=AssertionError("rejected body must not be parsed")), \
                     patch.object(sys, "argv", ["byteident.py"] + args), \
                     contextlib.redirect_stdout(io.StringIO()) as rendered:
                    self.assertEqual(b.main(), 0)
                    self.assertIn("NODATA", rendered.getvalue())
                    self.assertIn("unavailable", rendered.getvalue())
            data = json.loads(output.read_text())
            self.assertEqual(data["nodata"], 1)
            self.assertEqual(data["function_fidelity"]["example"]["candidate_bytes"], 12)
            self.assertEqual(data["instruction_runs"]["measured_functions"], 0)
            self.assertEqual(data["instruction_runs"]["unavailable_functions"], 1)

    def test_why_shows_all_copies_including_bad_copy_after_exact_copy(self):
        raw = bytes.fromhex("b80100000003430431c8c3")
        bad_raw = bytes.fromhex("b80200000003430431c8c3")
        def body(path, symbol):
            return (bad_raw if path == "bad.o" else raw), {}
        def instructions(path, symbol):
            return ([('mov', '$0x2,%eax')] + REFERENCE[1:] if path == "bad.o" else REFERENCE)
        with patch.object(b.glob, "glob", return_value=["a-good.o", "bad.o"]), \
             patch.object(b, "sizes", return_value={"example": len(raw)}), \
             patch.object(b, "body", side_effect=body), patch.object(b, "insns", side_effect=instructions), \
             patch.object(sys, "argv", ["byteident.py", "--why", "example"]), \
             contextlib.redirect_stdout(io.StringIO()) as rendered:
            self.assertEqual(b.main(), 0)
        text = rendered.getvalue()
        self.assertIn("2 defining copies; every copy shown", text)
        self.assertIn("a-good.o: fidelity EXACT", text)
        self.assertIn("bad.o: fidelity NONMATCH", text)
        self.assertIn("Conservative run across 2 copies: 3 / 4 (75.0%)", text)


if __name__ == '__main__':
    unittest.main()
