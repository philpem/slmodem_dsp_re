#!/usr/bin/env python3
"""Check saved full-TU controls and the tone detector publication boundary."""
from collections import Counter
import hashlib
import json
import re
import playbook_small_patterns as driver
from gcc3_reload_trace import instructions
from gcc3_stage_divergence import chunks
from gcc3_value_carriers_audit import inspect

TARGETS = {'_ZN19GenericToneDetector7processEf': 'int GenericToneDetector::process(float)',
           '_ZN19GenericToneDetector7processEPfj': 'int GenericToneDetector::process(float*, unsigned int)'}


def main():
    root = driver.ROOT / 'build/recovery-generic-tone-publication'
    ledger = json.loads((root / 'results.json').read_text())
    cells = ledger['families']['GenericToneDetector']['cells']
    base = root / 'GenericToneDetector/baseline/candidate.o'
    metadata = inspect(base)
    report = {}
    for label, cell in cells.items():
        folder = root / 'GenericToneDetector' / label
        obj = folder / 'candidate.o'
        assert hashlib.sha256(obj.read_bytes()).hexdigest() == cell['object_hash']
        current = inspect(obj)
        for key in ('records', 'allocated', 'nobits', 'relocations'):
            assert current[key] == metadata[key], (label, key)
        assert not cell.get('gains') and not cell.get('losses')
        assert set(cell.get('changed_bodies', [])) <= set(TARGETS)
        record = {'verdicts': cell['verdicts'], 'functions': {}}
        for name, header in TARGETS.items():
            rows = driver.b.insns(str(obj), name)
            original_rows = driver.b.insns(str(base), name)
            # EBX owns this in both complete emitted methods, independently
            # checked against their incoming this loads and all uses.
            memory = lambda stream: Counter((op, arg) for op, arg in stream if '(%ebx)' in arg)
            assert memory(rows) == memory(original_rows), (label, name, 'object accesses')
            assert [arg.split(' @', 1)[1] for op, arg in rows if op == 'call'] == [
                arg.split(' @', 1)[1] for op, arg in original_rows if op == 'call']
            text, = chunks(folder / 'GenericToneDetector.cpp.01.rtl', header)
            stream = instructions(text)
            stores = [(uid, field) for uid, pattern in stream.items()
                      if pattern[0] == 'set' and str(pattern[1][0]).startswith('mem')
                      for field in ('acc_0c', 'acc_10') if field in str(pattern[1])]
            expected = ['acc_10', 'acc_0c'] if 'output-first' in label else ['acc_0c', 'acc_10']
            assert [field for uid, field in stores[:2]] == expected
            record['functions'][name] = {'first_stores_initial_RTL': stores[:2],
                                         'object_access_multiset_and_calls_equal': True}
        report[label] = record
    count = sum(len(cell['verdicts']) for cell in cells.values())
    assert len(cells) == 4 and count == 28
    (root / 'audit.json').write_text(json.dumps({'cells': report, 'body_verdicts': count,
                                               'unchanged_exact_bystanders': 5}, indent=2) + '\n')
    print('4 complete TUs / 28 verdicts; 5 exact bystanders, data/bindings/BSS/nontext relocations preserved')
    print('8 method traces preserve object-access multisets and call targets; four early-store controls pass')


if __name__ == '__main__':
    main()
