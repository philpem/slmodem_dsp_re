#!/usr/bin/env python3
"""Decoded register-only SETcc controls through the production table verdict."""
import json
import os
import runpy
from pathlib import Path
import byteident as b
import jumptable as p

ROOT = Path(__file__).resolve().parents[2]
# Reuse the bounded ABI fixture and rerun its existing independent refusals.
abi = runpy.run_path(str(ROOT / 'tools/toolchain/jumptable_batch100_abi_controls.py'))
fixture, out = abi['fixture'], abi['OUT']
records = {}
for condition in ('o','no','b','ae','e','ne','be','a','s','ns','p','np','l','ge','le','g'):
    for register in ('al','cl','dl','bl','ah','ch','dh','bh'):
        label = 'setcc-' + condition + '-' + register
        obj, _ = fixture(label, change=lambda s, c=condition, r=register:
                         s.replace('.Lfirst:', '.Lfirst:\nset' + c + ' %' + r))
        proof = p.prove(str(obj), 'sample')
        body = b.body(str(obj), 'sample')
        assert b.verdict(*body, *body)[0] == 'EXACT', label
        records[label] = 'EXACT'
negative = {
    'saved-slot': 'sete 8(%esp)',
    'return-slot': 'sete 12(%esp)',
    'outgoing-slot': 'sete (%esp)',
    'ordinary-memory': 'sete (%ebx)',
    'indexed-stack': 'sete (%esp,%eax,1)',
    'operand-prefix': '.byte 0x66,0x0f,0x94,0xc0',
    'address-prefix': '.byte 0x67,0x0f,0x94,0xc0',
    'nonzero-extension': '.byte 0x0f,0x94,0xc8',
}
for label, instruction in negative.items():
    obj, _ = fixture('setcc-' + label, change=lambda s, x=instruction:
                     s.replace('.Lfirst:', '.Lfirst:\n' + x))
    try:
        p.prove(str(obj), 'sample')
    except p.Refused as error:
        records[label] = str(error)
    else:
        raise AssertionError('unsafe SETcc accepted: ' + label)
    body = b.body(str(obj), 'sample')
    assert b.verdict(*body, *body)[0] == 'UNRESOLVED', label
# BL/BH writes must invalidate a previous restore, even though only a byte
# changes. Existing ABI tracking owns this decision rather than SETcc syntax.
for register in ('bl', 'bh'):
    obj, _ = fixture('setcc-after-pop-' + register, change=lambda s, r=register:
                     s.replace('pop %ebx\nret', 'pop %ebx\nsete %' + r + '\nret'))
    try:
        p.prove(str(obj), 'sample')
    except p.Refused as error:
        records['after-pop-' + register] = str(error)
    else:
        raise AssertionError('callee restore clobber accepted')
    body = b.body(str(obj), 'sample')
    assert b.verdict(*body, *body)[0] == 'UNRESOLVED'
# A relocated field overlapping SETcc cannot be normalized away.
obj, _ = fixture('setcc-overlap-relocation', change=lambda s: s.replace(
    '.Lfirst:', '.Lfirst:\n.Lset:\nsete %al\nnop\n.reloc .Lset,R_386_32,message'))
try:
    p.prove(str(obj), 'sample')
except p.Refused as error:
    records['overlap-relocation'] = str(error)
else:
    raise AssertionError('overlapping SETcc relocation accepted')
body = b.body(str(obj), 'sample')
assert b.verdict(*body, *body)[0] == 'UNRESOLVED'
# The new instruction must not bypass guard dominance or table identity.
obj, _ = fixture('setcc-guard-bypass', change=lambda s: s.replace(
    '.Lfirst:', '.Lfirst:\nsete %al\njmp .Lbypass').replace(
    'jmp *.Ltable', '.Lbypass:\njmp *.Ltable'))
try:
    p.prove(str(obj), 'sample')
except p.Refused as error:
    records['guard-bypass'] = str(error)
else:
    raise AssertionError('SETcc guard bypass accepted')
body = b.body(str(obj), 'sample')
assert b.verdict(*body, *body)[0] == 'UNRESOLVED'
original, _ = fixture('setcc-targets-original', change=lambda s: s.replace(
    '.Lfirst:', '.Lfirst:\nsete %al'))
changed, _ = fixture('different-targets', change=lambda s: s.replace(
    '.Lfirst:', '.Lfirst:\nsete %al'))
# Use a fresh path: the inherited controls already measured different-targets.
unique_changed = out / 'setcc-altered-targets.o'
unique_changed.write_bytes(changed.read_bytes())
changed = unique_changed
assert b.verdict(*b.body(str(original), 'sample'),
                 *b.body(str(changed), 'sample'))[0] == 'RELOC'
records['changed-targets'] = 'RELOC'
blob = b.BLOB
objects = Path(os.environ['TC_OUT'])
current = objects / 'src_pump_v17_V17Rx.c.o'
if not current.exists():
    matches = [path for path in objects.glob('*.o') if 'RxNextStateV17' in b.sizes(str(path))]
    assert len(matches) == 1, matches
    current = matches[0]
real = [p.prove(str(path), 'RxNextStateV17') for path in (blob, current)]
for proof in real:
    assert proof['targets'] == [106,151,227,279,320,385,437]
    assert proof['fixed_stack_frame']['reachable_instructions'] == 168
    assert proof['fixed_stack_frame']['returns'] == 8
    assert proof['fixed_stack_frame']['direct_abi_calls'] == 11
assert b.verdict(*b.body(blob, 'RxNextStateV17'),
                 *b.body(str(current), 'RxNextStateV17'))[0] == 'EXACT'
(out / 'setcc-results.json').write_text(json.dumps({'controls': records, 'real': real}, indent=2) + '\n')
print('SETcc: 128 register positives EXACT; 12 unsafe/unsupported refusals UNRESOLVED; altered targets RELOC; real RxNextStateV17 pair EXACT; inherited 35 ABI controls passed')
