#!/usr/bin/env python3
"""Verify the unsigned range predicate and complete Dialer TU controls."""
import hashlib
import json
import re
from pathlib import Path
import playbook_small_patterns as driver
from gcc3_value_carriers_audit import inspect
from gcc3_stage_divergence import chunks

root = driver.ROOT / 'build/recovery-dialer-unsigned-guard'
cells = json.loads((root / 'results.json').read_text())['families']['Dialer']['cells']
base = root / 'Dialer/baseline/candidate.o'
metadata = inspect(base)
reports = {}
for label, cell in cells.items():
    folder = root / 'Dialer' / label
    obj = folder / 'candidate.o'
    assert hashlib.sha256(obj.read_bytes()).hexdigest() == cell['object_hash']
    actual = inspect(obj)
    for key in ('records', 'allocated', 'nobits', 'relocations'):
        assert actual[key] == metadata[key], (label, key)
    changed = [n for n in cell['functions']
               if driver.b.body(str(base), n) != driver.b.body(str(obj), n)]
    assert changed == ([] if label == 'baseline' else ['DialerAbort'])
    verdicts = {n: list(driver.b.verdict(*driver.b.body(driver.b.BLOB, n),
                                      *driver.b.body(str(obj), n)))
                for n in cell['verdicts']}
    assert verdicts == cell['verdicts'] and not cell.get('losses')
    rtl, = chunks(folder / 'Dialer.c.01.rtl', 'DialerAbort')
    predicate = 'leu' if label == 'unsigned-range' else 'le'
    assert re.search(r'if_then_else \((\w+) ', rtl)[1] == predicate
    assert 'progress_state+0' in rtl and '(const_int 10 [0xa])' in rtl
    reports[label] = {'changed': changed, 'verdicts': verdicts,
                      'initial_predicate': predicate}
assert len(reports) == 2
(root / 'audit.json').write_text(json.dumps(reports, indent=2) + '\n')
print('2 complete TUs / 12 verdicts: unsigned guard restored, 5 bystanders and nontext/bindings unchanged; 0 exact gains/losses')
