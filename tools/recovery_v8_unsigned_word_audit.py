#!/usr/bin/env python3
"""Audit the V8 word-load opcode correction without changing the shared field."""
import json
import hashlib
import playbook_small_patterns as d
from gcc3_value_carriers_audit import inspect

root = d.ROOT / 'build/recovery-v8-unsigned-word'
cells = json.loads((root / 'results.json').read_text())['families']['V8Interface']['cells']
base = root / 'V8Interface/baseline/candidate.o'
metadata = inspect(base)
reports = {}
for label, cell in cells.items():
    obj = root / 'V8Interface' / label / 'candidate.o'
    assert hashlib.sha256(obj.read_bytes()).hexdigest() == cell['object_hash']
    actual = inspect(obj)
    for key in ('records', 'allocated', 'nobits', 'relocations'):
        assert actual[key] == metadata[key], (label, key)
    changed = [n for n in cell['functions'] if d.b.body(str(base), n) != d.b.body(str(obj), n)]
    assert changed == ([] if label == 'baseline' else ['V8GetMessage'])
    verdicts = {n: list(d.b.verdict(*d.b.body(d.b.BLOB, n), *d.b.body(str(obj), n))) for n in cell['verdicts']}
    assert verdicts == cell['verdicts'] and not cell.get('losses') and not cell.get('gains')
    reports[label] = {'changed': changed, 'verdicts': verdicts}
a, ar = d.b.body(str(base), 'V8GetMessage')
b, br = d.b.body(str(root / 'V8Interface/unsigned-word/candidate.o'), 'V8GetMessage')
assert len(a) == len(b) and ar == br
delta = [i for i in range(len(a)) if a[i] != b[i]]
assert len(delta) == 1
pos, = delta
assert a[pos-1:pos+3] == bytes.fromhex('0f bf 14 5f')
assert b[pos-1:pos+3] == bytes.fromhex('0f b7 14 5f')
original, _ = d.b.body(d.b.BLOB, 'V8GetMessage')
assert bytes.fromhex('0f b7 14 5f d1 ea') in original
assert bytes.fromhex('0f b7 14 5f d1 ea') in b
assert bytes.fromhex('0f b7 14 5f d1 ea') not in a
(root / 'audit.json').write_text(json.dumps({'cells': reports, 'changed_opcode_offset': pos, 'original_load_and_shift_control': True}, indent=2) + '\n')
print('2 full TUs / 14 verdicts: one opcode byte restored, six bystanders and data/bindings unchanged; 0 exact gains/losses')
