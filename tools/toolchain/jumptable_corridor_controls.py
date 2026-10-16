#!/usr/bin/env python3
"""Bounded argument MOV corridor and push-only leaf frame proof controls."""
import json
import os
from pathlib import Path
import subprocess
import byteident as b
import jumptable as p
ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'build/jumptable-corridor-controls'
OUT.mkdir(parents=True, exist_ok=True)
BASE = '''.text
.globl sample
.type sample,@function
sample:
push %ebx
mov 8(%esp),%ebx
mov 12(%esp),%eax
cmp $1,%eax
mov 16(%esp),%edx
ja .Ldefault
.Ldispatch:
jmp *.Ltable(,%eax,4)
.Lfirst:
mov %edx,(%ebx)
mov $1,%eax
jmp .Lexit
.Lsecond:
mov %edx,4(%ebx)
mov $2,%eax
jmp .Lexit
.Ldefault:
xor %eax,%eax
.Lexit:
pop %ebx
ret
.size sample,.-sample
.section .rodata
.p2align 2
.Ltable:
.long .Lfirst,.Lsecond
.type datum,@object
datum: .long 0
.size datum,.-datum
.section .note.GNU-stack,"",@progbits
'''
def fixture(label, change=lambda s:s):
    src, obj = OUT / (label+'.s'), OUT / (label+'.o')
    src.write_text(change(BASE))
    subprocess.run(['as','--32','-o',str(obj),str(src)], check=True, capture_output=True)
    return obj
positive = {
    'baseline': lambda s:s,
    'no-corridor': lambda s:s.replace('mov 16(%esp),%edx\n',''),
    'disp32': lambda s:s.replace('mov 16(%esp),%edx','mov 132(%esp),%edx'),
    'two-saves': lambda s:s.replace('push %ebx','push %ebx\npush %esi').replace(
        'pop %ebx','pop %esi\npop %ebx').replace(
        'mov 16(%esp),%edx','mov 20(%esp),%edx').replace(
        'mov 12(%esp),%eax','mov 16(%esp),%eax').replace(
        'mov 8(%esp),%ebx','mov 12(%esp),%ebx'),
    'allocated': lambda s:s.replace('push %ebx','push %ebx\nsub $8,%esp').replace(
        'pop %ebx','add $8,%esp\npop %ebx').replace(
        'mov 8(%esp),%ebx','mov 16(%esp),%ebx').replace(
        'mov 12(%esp),%eax','mov 20(%esp),%eax').replace(
        'mov 16(%esp),%edx','mov 24(%esp),%edx'),
}
negative = {
    'unsaved-write': lambda s:s.replace('mov 16(%esp),%edx','mov 16(%esp),%esi'),
    'unaligned-load': lambda s:s.replace('mov 16(%esp),%edx','mov 17(%esp),%edx'),
    'incoming-corridor-relocation': lambda s:s.replace('mov 16(%esp),%edx','.Lload:\nmov 16(%esp),%edx').replace('.long .Lfirst,.Lsecond','.long .Lfirst,.Lsecond\n.long .Lload'),
    'selector-write': lambda s:s.replace('mov 16(%esp),%edx','mov 16(%esp),%eax'),
    'stack-write': lambda s:s.replace('mov 16(%esp),%edx','mov %edx,16(%esp)'),
    'flags-clobber': lambda s:s.replace('mov 16(%esp),%edx','add $1,%edx'),
    'partial-write': lambda s:s.replace('mov 16(%esp),%edx','movw 16(%esp),%dx'),
    'address-prefix': lambda s:s.replace('mov 16(%esp),%edx','.byte 0x67\nmov 16(%esp),%edx'),
    'two-loads': lambda s:s.replace('mov 16(%esp),%edx','mov 16(%esp),%edx\nmov 20(%esp),%ecx'),
    'indexed-load': lambda s:s.replace('mov 16(%esp),%edx','mov 16(%esp,%ecx,4),%edx'),
    'memory-load': lambda s:s.replace('mov 16(%esp),%edx','mov (%ebx),%edx'),
    'save-slot-read': lambda s:s.replace('mov 16(%esp),%edx','mov (%esp),%edx'),
    'return-read': lambda s:s.replace('mov 16(%esp),%edx','mov 4(%esp),%edx'),
    'save-slot-write': lambda s:s.replace('.Lfirst:', '.Lfirst:\nmov %edx,(%esp)'),
    'return-write': lambda s:s.replace('.Lfirst:', '.Lfirst:\nmov %edx,4(%esp)'),
    'frame-escape': lambda s:s.replace('.Lfirst:', '.Lfirst:\nlea (%esp),%eax'),
    'call': lambda s:s.replace('.Lfirst:', '.Lfirst:\ncall helper'),
    'zero-allocation': lambda s:s.replace('push %ebx','push %ebx\nsub $0,%esp'),
    'zero-release': lambda s:s.replace('pop %ebx','add $0,%esp\npop %ebx'),
    'post-pop-argument': lambda s:s.replace('pop %ebx','pop %ebx\nmov 16(%esp),%eax'),
    'wrong-pop': lambda s:s.replace('pop %ebx','pop %esi'),
    'missing-pop': lambda s:s.replace('pop %ebx\n',''),
    'clobber-after-pop': lambda s:s.replace('pop %ebx','pop %ebx\nxor %ebx,%ebx'),
    'guard-bypass': lambda s:s.replace('.Lfirst:', '.Lfirst:\njmp .Ldispatch'),
    'corridor-bypass': lambda s:s.replace('mov 16(%esp),%edx','.Lload:\nmov 16(%esp),%edx').replace('.Lfirst:', '.Lfirst:\njmp .Lload'),
    'relocated-load': lambda s:s.replace('mov 16(%esp),%edx','.Lload:\nmov 132(%esp),%edx\n.reloc .Lload+3,R_386_32,datum'),
    'overlap-relocation': lambda s:s.replace('mov 16(%esp),%edx','.Lload:\nmov 16(%esp),%edx\n.reloc .Lload,R_386_32,datum'),
}
records = {}
for label, change in positive.items():
    path = fixture(label, change)
    proof = p.prove(str(path), 'sample')
    body = b.body(str(path), 'sample')
    assert b.verdict(*body,*body)[0] == 'EXACT', label
    records[label] = proof
for label, change in negative.items():
    path = fixture(label, change)
    try: p.prove(str(path), 'sample')
    except p.Refused as error: records[label] = str(error)
    else: raise AssertionError('unsafe corridor/frame accepted: '+label)
    body = b.body(str(path), 'sample')
    assert b.verdict(*body,*body)[0] == 'UNRESOLVED', label
changed = fixture('altered-targets', lambda s:s.replace('.long .Lfirst,.Lsecond','.long .Lsecond,.Lfirst'))
assert b.verdict(*b.body(str(OUT/'baseline.o'), 'sample'), *b.body(str(changed), 'sample'))[0] == 'RELOC'
name = '_ZN21V90ConstellationPower20getConstellationInfoEP16V90MappingParams26V90TxPowerMeasurementPointj'
current = Path(os.environ['TC_OUT']) / 'src_pump_v90_V90ConstellationPower.cpp.o'
real = [p.prove(str(path), name) for path in (b.BLOB, current)]
for proof in real:
    assert proof['targets'] == [29,88,110,132,154,66]
    assert proof['fixed_stack_frame'] == {'bytes':0, 'pushed_registers':['ebx'],
        'reachable_instructions':65, 'returns':1, 'direct_abi_calls':0}
assert b.verdict(*b.body(b.BLOB,name), *b.body(str(current),name))[0] == 'EXACT'
(OUT/'results.json').write_text(json.dumps({'controls':records,'real':real},indent=2)+'\n')
print(f'{len(positive)} corridor/frame positives EXACT; {len(negative)} refusals UNRESOLVED; altered targets RELOC; real six-slot push-only pair EXACT')
