#!/usr/bin/env python3
"""Positive/equal historical controls and synthetic refusal checks for stage trace."""
import argparse
import hashlib
import json
import tempfile
from pathlib import Path
import sys
_TOOLS = Path(__file__).resolve().parent
sys.path[:] = [p for p in sys.path if Path(p or '.').resolve() != _TOOLS]
import dis  # avoid tools/dis.py shadowing the standard library
from unittest import mock
sys.path.insert(0, str(_TOOLS))
from gcc3_stage_divergence import compare, normalize, store_splits, structural_alignment, cgraph_summary
from gcc3_reload_trace import instructions


def check(condition, label):
    if not condition:
        raise AssertionError(label)


def insn(uid, value, reg=0):
    return '(insn %d 0 0 0 (set (reg:SI %d) (const_int %s)) 0 (nil) (nil))\n' % (uid, reg, value)


def validate(controls, output):
    records = []
    graph = 'Initial callgraph:\n\nfoo(): 7 insns needed tree inlinable\n  called by: caller\n  calls: bar\nfoo(): 8 insns\n  calls: baz\nInlined 1 calls, eliminated 0 functions, 10 insns turned to 9 insns.\n'
    parsed = cgraph_summary(graph)
    check(parsed['function_records'] == 2, 'duplicate printed clone names retained')
    check(parsed['phases']['Initial callgraph'][0]['edges']['calls'] == 'bar', 'callee preserved')
    check(parsed['phases'] != cgraph_summary(graph.replace('calls: bar', 'calls: other'))['phases'], 'callgraph callee negative control')
    check(parsed['phases'] != cgraph_summary(graph.replace('7 insns', '9 insns'))['phases'], 'callgraph size negative control')
    check(cgraph_summary('unrecognised format')['function_records'] == 0, 'unsupported callgraph has zero records')
    records.append({'control': 'callgraph-duplicates-callee-size-and-empty', 'passed': True})
    header = 'V90Parameters::V90Parameters(_tagModemParameters*)'
    before, after = ((controls / 'pre-434-retype', controls / 'post-434-retype')
                     if controls is not None else (None, None))
    historical = [('historical-positive', after, '28.peephole2'),
                  ('historical-equal', before, None)] if controls is not None else []
    for label, right, expected in historical:
        if not before.exists() or not right.exists():
            raise ValueError('historical controls absent: ' + str(controls))
        result = compare(before, right, header, all_clones=True)
        check(result['first_pattern_or_order_divergence'] == {'0': expected, '1': expected}, 'validation assertion at line 25')
        (output / (label + '.json')).write_text(json.dumps(result, indent=2) + '\n')
        records.append({'control': label, 'stage_pairs': result['stage_pairs'],
                        'streams': result['instruction_streams'], 'passed': True})
    with tempfile.TemporaryDirectory() as temporary:
        base = Path(temporary)
        left, right = base / 'left', base / 'right'
        left.mkdir(); right.mkdir()
        def write(folder, body, stage='01.rtl'):
            (folder / ('probe.c.' + stage)).write_text(';; Function probe\n' + body)
        # A real rewrite immediately after capture must not separate parsed
        # contents from the advertised hash (stream and nonstream formats).
        write(left, insn(1, 7)); write(right, insn(1, 7))
        graph_path = right / 'probe.c.00.cgraph'
        (left / graph_path.name).write_text(graph); graph_path.write_text(graph)
        stream_path = right / 'probe.c.01.rtl'
        captured = {stream_path: stream_path.read_bytes(), graph_path: graph_path.read_bytes()}
        replacements = {stream_path: (';; Function probe\n' + insn(1, 9)).encode(),
                        graph_path: graph.replace('calls: bar', 'calls: other').encode()}
        real_read = Path.read_bytes
        reads = {}
        def rewrite_after_read(path):
            data = real_read(path)
            if path in captured:
                reads[path] = reads.get(path, 0) + 1
                path.write_bytes(replacements[path])
            return data
        with mock.patch.object(Path, 'read_bytes', rewrite_after_read):
            result = compare(left, right, 'probe')
        check(result['stages'][0]['clones'][0]['equal'], 'stream uses captured bytes')
        check(result['stages'][0]['right_sha256'] == hashlib.sha256(captured[stream_path]).hexdigest(),
              'stream hash describes parsed contents')
        nonstream = result['nonstream_comparisons'][0]
        check(nonstream['status'] == 'raw-equal' and nonstream['callgraph']['printed_records_equal'],
              'cgraph parses the bytes it compared')
        check(nonstream['right_sha256'] == hashlib.sha256(captured[graph_path]).hexdigest(),
              'cgraph hash describes parsed contents')
        check(reads == {stream_path: 1, graph_path: 1}, 'one read per input dump')
        check(compare(left, right, 'probe')['first_pattern_or_order_divergence']['0'] == '01.rtl',
              'a fresh analysis detects changed stream')
        graph_path.unlink(); (left / graph_path.name).unlink()
        records.append({'control': 'read-once-stream-and-cgraph-hash-with-concurrent-rewrite', 'passed': True})
        a = insn(1, '0x12345678') + insn(2, 7)
        write(left, a)
        for label, body, divergent in [
            ('equal', a, False),
            ('large-hex-immediate', a.replace('0x12345678', '0x87654321'), True),
            ('register-colour', a.replace('reg:SI 0', 'reg:SI 2'), True),
            ('instruction-order', insn(2, 7) + insn(1, '0x12345678'), True),
            ('notes-only', a.replace('(nil) (nil)', '(nil) (expr_list:REG_DEAD (reg:SI 4) (nil))'), False),
        ]:
            write(right, body)
            result = compare(left, right, 'probe')
            check((result['first_pattern_or_order_divergence']['0'] is not None) == divergent, 'validation assertion at line 46')
            records.append({'control': label, 'passed': True})
        annotated = '(insn 3 0 0 0 (set (reg:SI 0) (symbol_ref:SI ("*.LC4") [flags 0x2] <string_cst 0x12345678>)) 0 (nil) (nil))\n'
        write(left, annotated)
        for label, body, divergent in [
            ('parsed-string-tree-address', annotated.replace('0x12345678>', '0x87654321>'), False),
            ('parsed-symbol-identity', annotated.replace('*.LC4', '*.LC5'), True),
            ('parsed-symbol-flags', annotated.replace('[flags 0x2]', '[flags 0x4]'), True),
            ('parsed-symbol-register', annotated.replace('reg:SI 0', 'reg:SI 2'), True),
        ]:
            write(right, body)
            result = compare(left, right, 'probe')
            check((result['first_pattern_or_order_divergence']['0'] is not None) == divergent, 'validation assertion at line 58')
            records.append({'control': label, 'passed': True})
        stack = '(insn 4 0 0 0 (set (reg:SI 0) (mem:SI (plus:SI (reg:SI 7) (const_int 28)))) 0 (nil) (nil))\n'
        write(left, stack)
        write(right, stack.replace('const_int 28', 'const_int 36'))
        check(compare(left, right, 'probe')['first_pattern_or_order_divergence']['0'] == '01.rtl', 'validation assertion at line 63')
        records.append({'control': 'parsed-stack-offset', 'passed': True})
        write(left, a)
        refusals = [
            ('duplicate-UID', lambda: instructions(insn(1, 0) + insn(1, 1))),
            ('truncated-RTL', lambda: instructions(insn(1, 0)[:-2])),
            ('missing-function', lambda: compare(left, right, 'absent')),
        ]
        write(right, a + ';; Function probe\n' + a)
        refusals.append(('ambiguous-clone', lambda: compare(left, right, 'probe')))
        for label, operation in refusals:
            try:
                operation()
            except ValueError:
                records.append({'control': label, 'passed': True})
            else:
                raise AssertionError('expected refusal: ' + label)
        write(right, a)
        write(right, a, '02.sibling')
        try:
            compare(left, right, 'probe')
        except ValueError:
            records.append({'control': 'unequal-stage-set', 'passed': True})
        else:
            raise AssertionError('expected unequal-stage refusal')
    check(normalize(['symbol_ref:SI', '<function_decl', '0x12345678']) == ['symbol_ref:SI', '<function_decl', '<tree-address>'], 'validation assertion at line 88')
    check(normalize(['const_int', '0x12345678']) == ['const_int', '0x12345678'], 'validation assertion at line 89')
    records.append({'control': 'only-tree-address-normalized', 'passed': True})
    old = instructions('(insn 1 0 0 0 (set (mem:SI (reg:SI 3)) (const_int 14)) 0 (nil) (nil))')
    new = instructions(insn(2, 14) + '(insn 3 0 0 0 (set (mem:SI (reg:SI 3)) (reg:SI 0)) 0 (nil) (nil))')
    check(len(store_splits(old, new)) == 1, 'validation assertion at line 93')
    wrong = instructions(insn(2, 13) + '(insn 3 0 0 0 (set (mem:SI (reg:SI 3)) (reg:SI 0)) 0 (nil) (nil))')
    check(not store_splits(old, wrong), 'validation assertion at line 95')
    records.append({'control': 'observable-store-split-positive-and-negative', 'passed': True})
    try:
        check(False, 'known failing negative control')
    except AssertionError:
        records.append({'control': 'validation-fails-under-optimized-python', 'passed': True})
    else:
        raise RuntimeError('validation guard disabled')
    annotated = instructions('(insn 1 0 0 0 (set (reg/v:SI 60 [ source_value ]) (const_int 1)))')
    altered = instructions('(insn 101 0 0 0 (set (reg/v:SI 61 [ source_value ]) (const_int 1)))')
    details = structural_alignment(annotated, altered, 8)['examples'][0]
    check(details['left_evidence']['retained_annotations'] and
          'source_value' in str(details['left_evidence']), 'retained source annotation')
    records.append({'control': 'source-annotation-retained', 'passed': True})
    alignment_controls = [
        ('uid-only', insn(1, 1, 60), insn(101, 1, 60), True, 'complete'),
        ('uid-and-value', insn(1, 1, 60), insn(101, 2, 60), False, 'complete'),
        ('uid-and-pseudo', insn(1, 1, 60), insn(101, 1, 61), False, 'complete'),
        ('uid-and-mode', insn(1, 1, 60), insn(101, 1, 60).replace(':SI', ':HI'), False, 'ambiguous-or-unmatched'),
        ('repeated-ambiguous', insn(1, 1) + insn(2, 2), insn(101, 3) + insn(102, 4), False, 'ambiguous-or-unmatched'),
        ('subreg-offset', '(insn 1 0 0 0 (set (reg:SI 60) (subreg:SI (reg:DI 61) 0)))',
         '(insn 101 0 0 0 (set (reg:SI 60) (subreg:SI (reg:DI 61) 4)))', False, 'complete'),
    ]
    for label, a, b, equal, status in alignment_controls:
        result = structural_alignment(instructions(a), instructions(b), 8)
        check(result['equal'] == equal and result['status'] == status, label)
        if label == 'uid-and-pseudo':
            check(result['examples'][0]['left_evidence']['register_operands'][0]['number'] == 60, label)
        records.append({'control': label, 'passed': True})
    result = {'controls_passed': len(records), 'controls_total': len(records), 'controls': records,
              'dynamic_scratch_state': 'not measured',
              'historical_controls': 'run' if controls is not None else 'not requested; no historical claim'}
    (output / 'validation.json').write_text(json.dumps(result, indent=2) + '\n')
    print('%d/%d detector controls passed; dynamic scratch state not measured' % (len(records), len(records)))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--controls', type=Path, help='optional historical pre/post-434 control directory')
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    validate(args.controls, args.output)


if __name__ == '__main__':
    main()
