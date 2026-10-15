#!/usr/bin/env python3
"""Validate four full-TU cursor controls and their publication boundaries."""
import json
import playbook_small_patterns as d
from gcc3_value_carriers_audit import inspect
from gcc3_stage_divergence import chunks, sets
from gcc3_reload_trace import instructions


def main():
    root = d.ROOT / 'build/residual-v8-cursor/V8Interface'
    family = json.loads((root.parent / 'results.json').read_text())['families']['V8Interface']
    base = root / 'baseline/candidate.o'
    metadata = inspect(base)
    rows = []
    verdicts = 0
    assert family['cells']['baseline']['baseline_reproduced']
    for label, cell in family['cells'].items():
        obj = root / label / 'candidate.o'
        actual = inspect(obj)
        assert all(actual[k] == metadata[k] for k in metadata if k != 'text_positions')
        changed = [n for n in cell['functions'] if d.b.body(str(obj), n) != d.b.body(str(base), n)]
        assert set(changed) <= {'V8Process'} and not cell.get('gains') and not cell.get('losses')
        stages = {}
        for stage in ('01.rtl', '24.lreg', '25.greg', '27.flow2', '28.peephole2', '30.rnreg', '35.mach'):
            selected = chunks(root / label / ('V8Interface.c.' + stage), 'V8Process')
            assert len(selected) == 1
            ins = instructions(selected[0])
            stores = {'tx_ring_base': [], 'tx_sym_b': []}
            for uid, p in ins.items():
                for x in sets(p):
                    if x[1][0].startswith('mem'):
                        for field in stores:
                            if any(field in str(t) for t in x[1][2:]):
                                stores[field].append(uid)
            stages[stage] = stores
        ring = 'ring-1' in label
        symbol = 'symbol-1' in label
        assert len(stages['01.rtl']['tx_ring_base']) == (1 if ring else 2)
        assert len(stages['01.rtl']['tx_sym_b']) == (1 if symbol else 2)
        assert len(stages['35.mach']['tx_ring_base']) == (1 if ring else 2)
        assert len(stages['35.mach']['tx_sym_b']) == 1
        rows.append({'cell': label, 'bytes': d.b.sizes(str(obj))['V8Process'],
                     'verdict': cell['verdicts']['V8Process'], 'publication_uids': stages,
                     'unchanged_bystanders': len(cell['functions']) - 1})
        verdicts += len(cell['verdicts'])
    assert len(rows) == 4 and verdicts == 28
    report = {'cells': rows, 'common_body_verdicts': verdicts, 'strict_gains': 0,
              'exact_losses': 0, 'source_adopted': False, 'metadata_nontext_equal': True}
    (d.ROOT / 'build/residual-v8-cursor-audit.json').write_text(json.dumps(report, indent=2) + '\n')
    for row in rows:
        print(row['cell'], row['bytes'], row['verdict'], row['publication_uids']['35.mach'])
    print('4 full-TU cells / 28 verdicts; publication controls fire; 6 bystanders preserved; no adoption')


if __name__ == '__main__':
    main()
