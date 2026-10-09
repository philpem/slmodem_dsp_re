#!/usr/bin/env python3
"""Validate counter operand/use recovery and discriminate actual store joins."""
import json
import re
from pathlib import Path
from branch_store_paths import analyze, destination, ALIASES
from gcc_x87_transfer_screen import assembly
from value_timing_tail_screen import operands
from gcc3_value_carriers_audit import inspect
import playbook_small_patterns as d

TARGETS = [('V27rx', 'V27RX_eq_train', 0x26), ('V27rx', 'V27RX_decision', 0x26),
           ('V29rx', 'V29RX_eq_train', 0x1e)]

def counter(path, symbol, offset):
    rows = assembly(path)[symbol]
    start = rows[0][0]
    for i, (_, op, arg) in enumerate(rows):
        fields = operands(arg)
        mem = destination(fields[0])
        if op != 'movzwl' or mem is None or mem[0] != offset:
            continue
        value = fields[1].removeprefix('%')
        aliases = ALIASES[value]
        candidates = [(j, row) for j, row in enumerate(rows[i+1:i+10], i+1)
                      if row[1] == 'cmp' and row[2] == '$0x8000,%'+aliases[1]]
        if len(candidates) != 1:
            continue
        j, _ = candidates[0]
        branch = rows[j+1]
        assert branch[1] == 'je'
        join = analyze(path, symbol, branch[0]-start, offset, mem[1])
        sequence = [(op, arg)] + [(mn, args) for _, mn, args in rows[i+1:j+1]
                                  if any(re.search(r'%'+alias+r'\b', args) for alias in aliases)]
        sequence += [('je', '<wrap>'), tuple(join['fallthrough']['instruction'])]
        def normalize(arg):
            arg = re.sub('%'+mem[1]+r'\b', '%OWNER32', arg)
            for alias, label in zip(aliases, ['VALUE32', 'VALUE16', 'VALUE8LO', 'VALUE8HI']):
                arg = re.sub('%'+alias+r'\b', '%'+label, arg)
            return arg
        sequence = [(mn, normalize(args)) for mn, args in sequence]
        assert join['taken']['instruction'][0] == 'movw'
        assert join['taken']['instruction'][1] == '$0x4000,0x%x(%%%s)' % (offset, mem[1])
        return {'sequence': sequence, 'join': join}
    raise AssertionError('counter load/word-compare boundary not found: '+symbol)

def main():
    rows, refused = [], []
    for family, symbol, offset in TARGETS:
        root = d.ROOT/'build/receiver-field-increment'/family
        paths = {'original': Path(d.b.BLOB), 'baseline': root/'baseline/candidate.o',
                 'candidate': root/('eq-1-decision-1' if family == 'V27rx' else 'eq-1-decision-0')/'candidate.o'}
        witnesses = {label: counter(path, symbol, offset) for label, path in paths.items()}
        assert witnesses['original']['sequence'] == witnesses['candidate']['sequence']
        assert witnesses['original']['sequence'] != witnesses['baseline']['sequence']
        assert not witnesses['original']['join']['same_store_instruction']
        assert not witnesses['candidate']['join']['same_store_instruction']
        rows.append({'symbol': symbol, 'witnesses': witnesses})
    base_refusals = []
    for label in ('baseline', 'eq-1-decision-1'):
        path = d.ROOT/'build/receiver-field-increment/V29rx'/label/'candidate.o'
        try:
            counter(path, 'V29RX_decision', 0x1e)
        except AssertionError as error:
            assert str(error) == 'base register changes'
            base_refusals.append(label)
        else:
            raise AssertionError('unsupported base-changing path unexpectedly accepted')
    positive = [analyze(Path(d.b.BLOB), 'SDMv27_init', 0xa, 0, 'eax'),
                analyze(Path(d.b.BLOB), 'pack_next_bit', 0x27, 0x154, 'ebx'),
                analyze(Path(d.b.BLOB), 'pack_next_bit', 0xd6, 0x15a, 'ebx')]
    assert all(row['same_store_instruction'] for row in positive)
    for offset, field in [(1, 0), (0, 0), (0xa, 0x7fff)]:
        try:
            analyze(Path(d.b.BLOB), 'SDMv27_init', offset, field, 'eax')
        except AssertionError:
            refused.append([offset, field])
        else:
            raise AssertionError('malformed control unexpectedly accepted')
    metadata = []
    for family in ('V27rx', 'V29rx'):
        root = d.ROOT/'build/receiver-field-increment'/family
        before, after = root/'baseline/candidate.o', root/('eq-1-decision-1' if family == 'V27rx' else 'eq-1-decision-0')/'candidate.o'
        a, b = inspect(before), inspect(after)
        assert all(a[key] == b[key] for key in a if key != 'text_positions')
        fs = d.b.sizes(str(before))
        assert set(fs) == set(d.b.sizes(str(after))) and len(fs) == 5
        changed = [n for n in fs if d.b.body(str(before), n) != d.b.body(str(after), n)]
        assert set(changed) == {row[1] for row in TARGETS if row[0] == family}
        source = (d.ROOT/('src/fax/'+family+'.c')).read_text()
        assert source.count('++dec->sym_count;') == (2 if family == 'V27rx' else 1)
        metadata.append({'family': family, 'changed_bodies': changed, 'unchanged_bystanders': len(fs)-len(changed)})
    out = d.ROOT/'build/receiver-counter-boundary-audit.json'
    out.write_text(json.dumps({'counters': rows, 'common_store_positives': positive,
                               'malformed_refusals': refused, 'base_change_refusals': base_refusals, 'metadata': metadata,
                               'strict_gains': 0}, indent=2)+'\n')
    print('3 original/candidate counter boundaries match; 3 baseline differences; 7 bystanders unchanged')
    print('branch-store controls: 3 common, 3 separate, 3 malformed + 2 base-change refusals')

if __name__ == '__main__':
    main()
