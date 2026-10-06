#!/usr/bin/env python3
"""Trace witnessed FSE reset stores across validated period RTL streams."""
import hashlib
import json
import playbook_small_patterns as d
from gcc3_reload_trace import function, instructions


def main():
    root = d.ROOT / 'build/fse-config-copy/fpm_fse/baseline'
    ledger = json.loads((root.parent.parent/'results.json').read_text())
    cell = ledger['families']['fpm_fse']['cells']['baseline']
    assert hashlib.sha256((root/'candidate.o').read_bytes()).hexdigest() == cell['object_hash']
    initial = instructions(function((root/'fpm_fse.c.01.rtl').read_text(), 'FPM_FSE_init'))
    tracked = {19: 'copy', 29: 'freq', 45: 'phase_acc'}
    assert 'mem/s:BLK' in repr(initial[19]) and '.cfg+0' in repr(initial[19])
    assert '.freq+0' in repr(initial[29]) and '.phase_acc+0' in repr(initial[45])
    rows, excluded = [], []
    for path in sorted(root.glob('fpm_fse.c.[0-9]*')):
        if path.suffix == '.cgraph':
            continue
        try:
            ins = instructions(function(path.read_text(), 'FPM_FSE_init'))
        except ValueError as error:
            excluded.append({'stage': path.name, 'reason': str(error)})
            continue
        assert set(tracked) <= set(ins), path
        order = [tracked[uid] for uid in ins if uid in tracked]
        rows.append({'stage': path.name, 'order': order})
    assert len(excluded) == 1 and excluded[0]['stage'].endswith('.08.gcse')
    first = next(row for row in rows if row['order'] != rows[0]['order'])
    assert rows[0]['order'] == ['copy', 'freq', 'phase_acc']
    assert first['stage'].endswith('.33.sched2')
    assert first['order'] == ['freq', 'phase_acc', 'copy']
    assert all(row['order'] == first['order'] for row in rows if row['stage'] >= first['stage'])
    result = {'tracked': tracked, 'parsed_stages': len(rows), 'excluded': excluded,
              'first_order_change': first, 'rows': rows,
              'scope': 'Instruction-stream order, not recovered scheduler causality or original source.'}
    (d.ROOT/'build/fse-copy-stage-trace.json').write_text(json.dumps(result, indent=2)+'\n')
    print('FSE trace:', len(rows), 'validated stages, 1 explicitly excluded duplicate stream;',
          'first reorder in sched2; known store-motion detector fired')


if __name__ == '__main__':
    main()
