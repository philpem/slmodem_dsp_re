#!/usr/bin/env python3
"""Gentoo controls for a private frame argument and a checked leaf callee."""
import argparse
import json
import subprocess
from pathlib import Path
import callee_effects_controls as leaf
import jumptable as j

ROOT=Path(__file__).resolve().parents[2]
CALLER='''.text
.globl caller
.type caller,@function
caller:
sub $32,%esp
mov %ebx,28(%esp)
mov 36(%esp),%ebx
mov (%ebx),%eax
cmp $1,%eax
ja .Lexit
jmp *.Ltable(,%eax,4)
.Lfirst:
lea 16(%esp),%eax
mov %ebx,(%esp)
mov %eax,4(%esp)
.Lcall:
call sample
mov $0,%ecx
movl $0,4(%esp)
.Lread:
testb $1,17(%esp)
je .Lexit
jmp .Lexit
.Lsecond:
jmp .Lexit
.Lexit:
mov 28(%esp),%ebx
add $32,%esp
ret
.size caller,.-caller
.section .rodata
.p2align 2
.Ltable:
.long .Lfirst,.Lsecond
'''
HELPER=leaf.SOURCE[:leaf.SOURCE.index('.globl caller')]
SOURCE=HELPER+CALLER


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--out',type=Path,default=ROOT/'scratch/callee-effects/frame-controls')
    args=ap.parse_args(); out=args.out.resolve(); out.mkdir(parents=True,exist_ok=True)
    relative=out.relative_to(ROOT)
    positive={
        'baseline':SOURCE,
        'fixed-frame-setcc':SOURCE.replace('.Lfirst:\n','.Lfirst:\nsete %al\n'),
        'rebased':SOURCE.replace('.text\n','.text\n.space 16,0x90\n',1),
        'larger-safe-write':SOURCE.replace('mov $1,%eax\nret','movb $3,8(%ecx)\nmov $1,%eax\nret'),
        'reordered-kills':SOURCE.replace('mov $0,%ecx\nmovl $0,4(%esp)', 'movl $0,4(%esp)\nmov $0,%ecx'),
    }
    negative={
        'fixed-frame-memory-setcc':SOURCE.replace('.Lfirst:\n','.Lfirst:\nsete 16(%esp)\n'),
        'fixed-frame-prefixed-setcc':SOURCE.replace('.Lfirst:\n','.Lfirst:\n.byte 0x66\nsete %al\n'),
        'external-direct-call-entry':SOURCE+'\n.text\n.globl intruder\n.type intruder,@function\nintruder: jmp .Lcall\n.size intruder,.-intruder\n',
        'external-direct-read-entry':SOURCE+'\n.text\n.globl intruder\n.type intruder,@function\nintruder: jmp .Lread\n.size intruder,.-intruder\n',
        'relocated-call-entry':SOURCE+'\n.data\n.long .Lcall\n',
        'relocated-read-entry':SOURCE+'\n.data\n.long .Lread\n',
        'overwritten-pointer-argument':SOURCE.replace('.Lcall:\n','.Lcall:\nmov %ebx,4(%esp)\n'),
        'partial-overwritten-pointer-argument':SOURCE.replace('.Lcall:\n','.Lcall:\nmov %bx,6(%esp)\n'),
        'later-input-argument-alias':SOURCE.replace('.Lcall:\n','.Lcall:\nmov %eax,(%esp)\n'),
        'missing-input-argument':SOURCE.replace('mov %ebx,(%esp)','nop'),
        'saved-state-alias':SOURCE.replace('lea 16(%esp),%eax','lea 28(%esp),%eax'),
        'return-word-alias':SOURCE.replace('lea 16(%esp),%eax','lea 32(%esp),%eax'),
        'outgoing-alias':SOURCE.replace('lea 16(%esp),%eax','lea 4(%esp),%eax'),
        'outside-envelope':SOURCE.replace('mov $1,%eax\nret','movb $3,12(%ecx)\nmov $1,%eax\nret'),
        'second-frame-capability':SOURCE.replace('mov %ebx,(%esp)','lea 20(%esp),%edx\nmov %ebx,(%esp)'),
        'pointer-copy':SOURCE.replace('mov %ebx,(%esp)','mov %eax,%edx\nmov %ebx,(%esp)'),
        'pointer-partial-mutation':SOURCE.replace('mov %ebx,(%esp)','mov $0,%al\nmov %ebx,(%esp)'),
        'implicit-pointer-mutation':SOURCE.replace('mov %ebx,(%esp)','mull %ebx\nmov %ebx,(%esp)'),
        'duplicate-pointer-argument':SOURCE.replace('mov %ebx,(%esp)','mov %eax,(%esp)'),
        'argument-slot-mismatch':SOURCE.replace('mov %eax,4(%esp)','mov %eax,8(%esp)'),
        'callee-retained-register':SOURCE.replace('mov $0,%ecx','nop').replace('movl $0,4(%esp)','movl $0,4(%esp)\ncall unknown'),
        'partial-retained-register-kill':SOURCE.replace('mov $0,%ecx','mov $0,%cl'),
        'retained-register-escape':SOURCE.replace('mov $0,%ecx','mov %ecx,(%ebx)\nmov $0,%ecx'),
        'stale-outgoing-pointer':SOURCE.replace('movl $0,4(%esp)','call unknown\nmovl $0,4(%esp)'),
        'stale-pointer-at-return':SOURCE.replace('movl $0,4(%esp)','nop'),
        'missing-output-write':SOURCE.replace('mov %dl,1(%ecx)','nop'),
        'preserved-bit-read':SOURCE.replace('testb $1,17(%esp)','testb $2,17(%esp)'),
        'read-before-call':SOURCE.replace('.Lcall:\n','.Lcall:\ntestb $1,17(%esp)\n'),
        'read-bypass':SOURCE.replace('.Lsecond:\njmp .Lexit','.Lsecond:\njmp .Lread'),
        'argument-bypass':SOURCE.replace('.Lsecond:\njmp .Lexit','.Lsecond:\njmp .Lcall'),
        'wrong-callee':SOURCE.replace('call sample','call unknown'),
        'wrong-addend':SOURCE.replace('call sample','call sample+4'),
        'callee-new-call':SOURCE.replace('mov $1,%eax\nret','call unknown\nmov $1,%eax\nret'),
        'incoming-pointer-retained':SOURCE.replace('mov $1,%eax\nret','nop\nret').replace('mov 4(%esp),%eax\nmovzbl (%eax),%edx','mov 4(%esp),%edx\nmovzbl (%edx),%edx'),
    }
    all_sources={**positive,**negative}
    for label,source in all_sources.items(): (out/(label+'.s')).write_text(source)
    script='set -eu\nas --version\n'+'\n'.join(
        f'as --32 -o /work/{relative}/{label}.o /work/{relative}/{label}.s' for label in all_sources)
    command=['docker','run','--rm','-v',str(ROOT)+':/work',leaf.IMAGE,'/bin/sh','-c',script]
    assembled=subprocess.run(command,check=True,capture_output=True,text=True)
    records={}
    for label in all_sources:
        try:
            proof=j.prove(str(out/(label+'.o')),'caller'); accepted=True; reason=None
        except j.Refused as error:
            proof=None; accepted=False; reason=str(error)
        assert accepted==(label in positive),(label,accepted,reason)
        records[label]={'accepted':accepted,'reason':reason,'proof':proof}
    (out/'results.json').write_text(json.dumps({'controls':records,'positive':len(positive),
        'negative':len(negative),'command':command,'assembler':assembled.stdout},indent=2)+'\n')
    print(f'{len(positive)} positive frame/callee controls; {len(negative)} adversarial refusals')


if __name__=='__main__': main()
