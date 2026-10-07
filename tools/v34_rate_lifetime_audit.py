#!/usr/bin/env python3
"""Audit the crossed RX byte recovery and closed TX controls as full TUs."""
import hashlib
import json
from pathlib import Path
import playbook_small_patterns as d
from gcc3_value_carriers_audit import inspect
from gcc3_stage_divergence import chunks

RX = 'VPcmV34GetCurrentRxBitRate'
TX = 'VPcmV34GetCurrentTxBitRate'
HEADER = 'int VPcmV34GetCurrentRxBitRate(void*)'


def main():
    count = grades = 0
    ledger = {}
    for family, expected in [('v34-rxrate-lifetime', 4), ('v34-txrate-owner', 4),
                             ('v34-txrate-result', 5)]:
        root = d.ROOT/'build'/family
        report = json.loads((root/'results.json').read_text())
        assert report['revision'] == 'b3665999'
        cells = report['families']['VPcmV34Main']['cells']
        assert len(cells) == expected
        base = root/'VPcmV34Main/baseline/candidate.o'
        assert base.read_bytes() == (root/'VPcmV34Main/retained.o').read_bytes()
        metadata = inspect(base)
        assert len(d.b.sizes(str(base))) == 57
        rows = []
        for label, cell in cells.items():
            obj = root/'VPcmV34Main'/label/'candidate.o'
            assert cell['compile_exit'] == 0 and '-DDSPLIB_REPRODUCE_BUGS' in cell['command'][-1]
            assert hashlib.sha256(obj.read_bytes()).hexdigest() == cell['object_hash']
            other = inspect(obj)
            assert all(other[k] == v for k, v in metadata.items() if k != 'text_positions')
            assert cell['functions'] == cells['baseline']['functions']
            assert cell['globals'] == cells['baseline']['globals']
            actual = {n: list(d.b.verdict(*d.b.body(d.b.BLOB, n), *d.b.body(str(obj), n)))
                      for n in cell['verdicts']}
            assert actual == cell['verdicts'] and len(actual) == 57
            changed = [n for n in cell['functions'] if d.b.body(str(obj), n) != d.b.body(str(base), n)]
            assert set(changed) <= {RX, TX}
            if label != 'baseline':
                assert changed == cell['changed_bodies']
                assert not cell['losses'] and set(cell['gains']) <= {RX}
            rows.append({'cell': label, 'rx': actual[RX], 'tx': actual[TX], 'changed_bodies': changed})
            count += 1
            grades += len(actual)
        ledger[family] = rows
    assert count == 13 and grades == 741
    rx = d.ROOT/'build/v34-rxrate-lifetime/VPcmV34Main'
    stages = {}
    for label in ('baseline', 'eager-1-common-0', 'eager-0-common-1', 'eager-1-common-1'):
        initial = chunks(rx/label/'VPcmV34Main.cpp.01.rtl', HEADER)[0]
        before_guard = initial.split('(jump_insn', 1)[0]
        eager = '.p3548' in before_guard and '.pac18' in before_guard
        sibling = chunks(rx/label/'VPcmV34Main.cpp.02.sibling', HEADER)[0].count('call_insn/j')
        assert eager == ('eager-1' in label)
        assert sibling == (0 if 'common-1' in label else 1)
        stages[label] = {'both_pointer_loads_before_guard': eager, 'sibling_calls': sibling}
    winner = rx/'eager-1-common-1/candidate.o'
    assert d.b.verdict(*d.b.body(d.b.BLOB, RX), *d.b.body(str(winner), RX)) == ('EXACT', 0)
    assert d.b.sizes(d.b.BLOB)[RX] == d.b.sizes(str(winner))[RX] == 90
    changed = [n for n in d.b.sizes(str(winner))
               if d.b.body(str(winner), n) != d.b.body(str(rx/'baseline/candidate.o'), n)]
    assert changed == [RX]
    owner = d.ROOT/'build/v34-txrate-owner/VPcmV34Main'
    result = d.ROOT/'build/v34-txrate-result/VPcmV34Main'
    assert winner.read_bytes() == (owner/'rx-1-tx-0/candidate.o').read_bytes()
    assert winner.read_bytes() == (result/'direct-0-result-0/candidate.o').read_bytes()
    assert (owner/'rx-1-tx-1/candidate.o').read_bytes() == (result/'direct-1-result-0/candidate.o').read_bytes()
    out = {'full_TU_compiles': count, 'body_verdicts': grades, 'cells': ledger,
           'rx_initial_and_sibling_stages': stages, 'adopted_gain': RX,
           'gain_original_bytes': 90, 'adopted_unchanged_bystanders': 56,
           'adopted_losses': [], 'new_control_subobject': False}
    (d.ROOT/'build/v34-rate-lifetime-audit.json').write_text(json.dumps(out, indent=2)+'\n')
    print('13 full-TU controls / 741 verdicts; raw repeats, bindings and nontext metadata pass')
    print('RX90B EXACT only with both axes; 56 unchanged bystanders; TX domain closed without gain')
    print('Initial RTL eager loads and sibling-pass call decisions discriminate all four RX controls')


if __name__ == '__main__':
    main()
