#!/usr/bin/env python3
"""Assembled controls for fixed-save ABI frames and relocated indexed loads."""
import argparse
import json
import subprocess
from pathlib import Path
import byteident as b
import jumptable as p

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'build/jumptable-fixed-abi-controls'
SOURCE = '''.text
.globl sample
.type sample,@function
sample:
sub $16,%esp
mov %ebx,8(%esp)
mov %esi,12(%esp)
mov 20(%esp),%esi
mov (%esi),%eax
cmp $1,%eax
ja .Ldefault
jmp *.Ltable(,%eax,4)
.Lfirst:
mov %esi,(%esp)
movl $message,4(%esp)
call helper
cwtl
movswl table(%eax,%eax,1),%ebx
movzwl table(%eax,%eax,1),%eax
jmp .Lexit
.Lsecond:
mov %esi,(%esp)
call helper
jmp .Lexit
.Ldefault:
xor %eax,%eax
.Lexit:
mov 8(%esp),%ebx
mov 12(%esp),%esi
add $16,%esp
ret
.size sample,.-sample
.type helper,@function
.section .rodata
.p2align 2
.Ltable:
.long .Lfirst,.Lsecond
.type message,@object
message:
.asciz "message"
.size message,.-message
.type table,@object
table:
.short 1,2,3,4
.size table,.-table
.section .note.GNU-stack,"",@progbits
'''


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--candidate', type=Path, required=True)
    args = ap.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    positive = {
        'baseline': SOURCE,
        'rebased': SOURCE.replace('.Ltable:\n', '.space 32\n.Ltable:\n'),
        'swapped-targets': SOURCE.replace('.long .Lfirst,.Lsecond', '.long .Lsecond,.Lfirst'),
    }
    negative = {
        'outgoing-save-overlap': SOURCE.replace('mov %esi,(%esp)', 'mov %esi,8(%esp)'),
        'outgoing-return-overlap': SOURCE.replace('mov %esi,(%esp)', 'mov %esi,16(%esp)'),
        'outgoing-unaligned': SOURCE.replace('mov %esi,(%esp)', 'mov %esi,1(%esp)'),
        'partial-outgoing-immediate': SOURCE.replace('movl $message,4(%esp)', 'movw $3,4(%esp)'),
        'byte-outgoing-immediate': SOURCE.replace('movl $message,4(%esp)', 'movb $3,4(%esp)'),
        'outgoing-saved-read': SOURCE.replace('cwtl', 'mov 4(%esp),%eax'),
        'wrong-restoration': SOURCE.replace('mov 8(%esp),%ebx', 'mov 12(%esp),%ebx'),
        'skip-save': SOURCE.replace('mov %ebx,8(%esp)', 'jmp .Lskip\nmov %ebx,8(%esp)\n.Lskip:'),
        'skip-restore': SOURCE.replace('mov 8(%esp),%ebx', 'nop'),
        'duplicate-save': SOURCE.replace('mov %ebx,8(%esp)', 'mov %ebx,8(%esp)\nmov %ebx,8(%esp)'),
        'save-after-write': SOURCE.replace('mov %ebx,8(%esp)', 'inc %ebx\nmov %ebx,8(%esp)'),
        'unsaved-callee-write': SOURCE.replace('cwtl', 'xor %edi,%edi'),
        'post-restore-clobber': SOURCE.replace('add $16,%esp', 'mov $2,%bl\nadd $16,%esp'),
        'frame-escape': SOURCE.replace('cwtl', 'lea (%esp),%eax'),
        'partial-esp': SOURCE.replace('cwtl', 'inc %sp'),
        'changed-depth': SOURCE.replace('cwtl', 'sub $16,%esp'),
        'call-after-release': SOURCE.replace('add $16,%esp', 'add $16,%esp\ncall helper'),
        'outgoing-after-release': SOURCE.replace('add $16,%esp', 'add $16,%esp\nmov %eax,(%esp)'),
        'unbalanced-return': SOURCE.replace('add $16,%esp', 'nop'),
        'call-addend': SOURCE.replace('call helper', 'call helper+4'),
        'call-object': SOURCE.replace('.type helper,@function', '.type helper,@object'),
        'indirect-call': SOURCE.replace('call helper', 'call *%eax'),
        'local-call': SOURCE.replace('.type helper,@function', '.type helper,@function\nhelper: ret\n.size helper,.-helper'),
        'relocated-stack-displacement': SOURCE.replace('cwtl', 'mov table(%esp),%eax'),
        'relocated-indexed-stack-base': SOURCE.replace('table(%eax,%eax,1)', 'table(%esp,%eax,1)'),
        'relocated-indexed-esp-destination': SOURCE.replace('table(%eax,%eax,1),%ebx', 'table(%eax,%eax,1),%esp'),
        'relocated-load-opcode': SOURCE.replace('movswl table(%eax,%eax,1),%ebx', 'mov table(%eax,%eax,1),%ebx'),
        'relocated-word-prefix': SOURCE.replace('movswl table(%eax,%eax,1),%ebx', 'data16 movswl table(%eax,%eax,1),%ebx'),
        'relocated-guard': SOURCE.replace('cmp $1,%eax', 'cmp $table,%eax'),
        'guard-bypass': SOURCE.replace('.Lfirst:', '.Lfirst:\njmp .Lbypass').replace('jmp *.Ltable', '.Lbypass:\njmp *.Ltable'),
        'return-immediate': SOURCE.replace('\nret\n', '\nret $4\n'),
        'implicit-callee-write': SOURCE.replace('cwtl', 'cpuid'),
    }
    records = {}; objects = {}
    for label, source in {**positive, **negative}.items():
        asm = OUT / (label + '.s'); obj = OUT / (label + '.o')
        asm.write_text(source)
        command = ['as', '--32', '-o', str(obj), str(asm)]
        subprocess.run(command, check=True, capture_output=True)
        objects[label] = obj
        try:
            proof = p.prove(str(obj), 'sample'); accepted = True; reason = None
        except p.Refused as error:
            proof = None; accepted = False; reason = str(error)
        assert accepted == (label in positive), (label, reason)
        own = b.body(str(obj), 'sample')
        grade = b.verdict(*own, *own)[0]
        assert grade == ('EXACT' if accepted else 'UNRESOLVED'), (label, grade)
        records[label] = {'accepted': accepted, 'reason': reason, 'proof': proof, 'self_grade': grade, 'command': command}
    original = b.body(str(objects['baseline']), 'sample')
    for label, expected in [('rebased', 'EXACT'), ('swapped-targets', 'RELOC')]:
        assert b.verdict(*original, *b.body(str(objects[label]), 'sample'))[0] == expected
    name = 'V32LocLoopNextState'; blob = ROOT / 'ref/slmodemd/dsplibs.o'
    a = p.prove(str(blob), name); c = p.prove(str(args.candidate), name)
    assert a['identity'] == c['identity'] and a['count'] == 34
    assert a['fixed_stack_frame']['outgoing_slots'] == [0,4,8,12,16]
    assert a['fixed_stack_frame']['direct_abi_calls'] == 17
    assert b.verdict(*b.body(str(blob), name), *b.body(str(args.candidate), name)) == ('EXACT',0)
    (OUT / 'results.json').write_text(json.dumps({'controls': records, 'real': {'original': a, 'candidate': c}, 'assembler': subprocess.check_output(['as', '--version'], text=True)}, indent=2) + '\n')
    print(f'{len(positive)} positive fixed-ABI controls; {len(negative)} adversarial refusals; rebased EXACT, altered targets RELOC; real 34-slot/17-call pair EXACT')


if __name__ == '__main__':
    main()
