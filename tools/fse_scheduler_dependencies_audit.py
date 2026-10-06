#!/usr/bin/env python3
"""Require diagnostic raw repeats and measured FSE scheduler dependency edges."""
import hashlib
import json
import re
import playbook_small_patterns as d
from gcc3_value_carriers_audit import inspect
from gcc3_reload_trace import function, instructions

HASHES = {
    'baseline': '6a3fde3e9fe67b2dcd9a3cd03f6d75ebd11109d9af0b5d004152b2380dcc4e64',
    'memcpy-1-delayed-1': '65126560951a098b1d64fc997d4d5bba8d5c2b5873f68bf4c426717fe88371d3',
}


def dependency_rows(text):
    head = text[:text.index('Ready list after')]
    rows = {}
    for line in head.splitlines():
        match = re.match(r'^;;\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+.*?:\s*([0-9 ]*)\s*$', line)
        if match:
            uid, code, block, incoming, priority, cost, outgoing = match.groups()
            assert int(uid) not in rows
            rows[int(uid)] = {'block': int(block), 'incoming': int(incoming),
                              'priority': int(priority), 'cost': int(cost),
                              'outgoing': [int(x) for x in outgoing.split()]}
    assert rows, 'no dependency table'
    return rows


def main():
    root = d.ROOT/'build/fse-scheduler-dependencies/fpm_fse'
    ledger = json.loads((root.parent/'results.json').read_text())
    base = inspect(root/'baseline/candidate.o')
    report = {}
    for label, cell in ledger['families']['fpm_fse']['cells'].items():
        obj = root/label/'candidate.o'
        digest = hashlib.sha256(obj.read_bytes()).hexdigest()
        assert digest == HASHES[label] == cell['object_hash'], (label, 'raw repeat')
        assert hashlib.sha256((obj.parent/'fpm_fse.c').read_bytes()).hexdigest() == cell['source_hash']
        observed = inspect(obj)
        for key in ('records', 'allocated', 'nobits', 'relocations'):
            assert observed[key] == base[key]
        for name, expected in cell['verdicts'].items():
            assert list(d.b.verdict(*d.b.body(d.b.BLOB, name), *d.b.body(str(obj), name))) == expected
        assert not cell.get('gains') and not cell.get('losses')
        before = instructions(function((obj.parent/'fpm_fse.c.31.bbro').read_text(), 'FPM_FSE_init'))
        copy = 19 if label == 'baseline' else 27
        assert 'mem/s:BLK' in repr(before[copy]) if copy == 19 else 'mem:BLK' in repr(before[copy])
        assert '.freq+0' in repr(before[29]) and '.phase_acc+0' in repr(before[45])
        text = function((obj.parent/'fpm_fse.c.33.sched2').read_text(), 'FPM_FSE_init')
        rows = dependency_rows(text)
        issued = [int(x) for x in re.findall(r'scheduling insn <<<(\d+)>>>', text)]
        edges = {uid: uid in rows[copy]['outgoing'] for uid in (29, 45, 31)}
        expected = {29: copy == 27, 45: copy == 27, 31: True}
        assert edges == expected
        order = [uid for uid in issued if uid in (copy, 29, 45)]
        assert order == ([29, 45, 19] if copy == 19 else [27, 29, 45])
        assert rows[29]['priority'] == rows[45]['priority'] == 6
        assert rows[copy]['priority'] == (5 if copy == 19 else 8)
        report[label] = {'copy_uid': copy, 'copy': rows[copy], 'freq': rows[29],
                         'phase_acc': rows[45], 'copy_edges': edges, 'issue_order': order}
    result = {'cells': 2, 'body_comparisons': 8, 'raw_repeats': 2, 'gains': 0, 'losses': 0,
              'dependency_detector': 'two absent SI edges become present; HI edge stays present',
              'rows': report}
    (root.parent/'audit.json').write_text(json.dumps(result, indent=2)+'\n')
    print('FSE dependency audit: 2 raw repeats/8 live body grades; copy-to-SI edges 0→2, HI edge retained; priority/issue predictions pass; no gains/losses')


if __name__ == '__main__':
    main()
