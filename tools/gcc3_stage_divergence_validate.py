#!/usr/bin/env python3
"""Positive/equal historical controls and synthetic refusal checks for stage trace."""
import argparse
import json
import tempfile
from pathlib import Path
from gcc3_stage_divergence import compare, normalize, store_splits
from gcc3_reload_trace import instructions


def insn(uid, value, reg=0):
    return '(insn %d 0 0 0 (set (reg:SI %d) (const_int %s)) 0 (nil) (nil))\n' % (uid, reg, value)


def validate(controls, output):
    records = []
    header = 'V90Parameters::V90Parameters(_tagModemParameters*)'
    before, after = controls / 'pre-434-retype', controls / 'post-434-retype'
    for label, right, expected in [('historical-positive', after, '28.peephole2'),
                                   ('historical-equal', before, None)]:
        result = compare(before, right, header, all_clones=True)
        assert result['first_pattern_or_order_divergence'] == {'0': expected, '1': expected}
        (output / (label + '.json')).write_text(json.dumps(result, indent=2) + '\n')
        records.append({'control': label, 'stage_pairs': result['stage_pairs'],
                        'streams': result['instruction_streams'], 'passed': True})
    with tempfile.TemporaryDirectory() as temporary:
        base = Path(temporary)
        left, right = base / 'left', base / 'right'
        left.mkdir(); right.mkdir()
        def write(folder, body, stage='01.rtl'):
            (folder / ('probe.c.' + stage)).write_text(';; Function probe\n' + body)
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
            assert (result['first_pattern_or_order_divergence']['0'] is not None) == divergent
            records.append({'control': label, 'passed': True})
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
    assert normalize(['symbol_ref:SI', '<function_decl', '0x12345678']) == ['symbol_ref:SI', '<function_decl', '<tree-address>']
    assert normalize(['const_int', '0x12345678']) == ['const_int', '0x12345678']
    records.append({'control': 'only-tree-address-normalized', 'passed': True})
    old = instructions('(insn 1 0 0 0 (set (mem:SI (reg:SI 3)) (const_int 14)) 0 (nil) (nil))')
    new = instructions(insn(2, 14) + '(insn 3 0 0 0 (set (mem:SI (reg:SI 3)) (reg:SI 0)) 0 (nil) (nil))')
    assert len(store_splits(old, new)) == 1
    wrong = instructions(insn(2, 13) + '(insn 3 0 0 0 (set (mem:SI (reg:SI 3)) (reg:SI 0)) 0 (nil) (nil))')
    assert not store_splits(old, wrong)
    records.append({'control': 'observable-store-split-positive-and-negative', 'passed': True})
    result = {'controls_passed': len(records), 'controls_total': len(records), 'controls': records,
              'dynamic_scratch_state': 'not measured'}
    (output / 'validation.json').write_text(json.dumps(result, indent=2) + '\n')
    print('%d/%d detector controls passed; dynamic scratch state not measured' % (len(records), len(records)))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--controls', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    validate(args.controls, args.output)


if __name__ == '__main__':
    main()
