#!/usr/bin/env python3
"""Positive and negative ELF fixtures for bounded induction-table proof."""
import json
import subprocess
import tempfile
from pathlib import Path
import induction_jumptable as p
import jumptable as j

SOURCE = '''.text
.globl fixture
.type fixture,@function
fixture:
 push %edi
 xor %edi,%edi
.Ldispatch:
 jmp *.Ltable(,%edi,4)
.Lzero:
 mov $3,%eax
 jmp .Ladvance
.Lone:
 mov $4,%eax
.Ladvance:
 inc %edi
 cmp $1,%edi
 jbe .Ldispatch
 pop %edi
 ret
.size fixture,.-fixture
.section .rodata
.p2align 2
.Ltable:
 .long .Lzero,.Lone
'''


def main():
    report = {}
    with tempfile.TemporaryDirectory(prefix='induction-table-') as folder:
        folder = Path(folder)
        forms = {
            'positive':SOURCE,
            'unknown-entry':SOURCE.replace(' xor %edi,%edi',' mov %eax,%edi'),
            'out-of-range-entry':SOURCE.replace(' xor %edi,%edi',' mov $2,%edi'),
            'changed-target':SOURCE.replace('.long .Lzero,.Lone','.long .Lone,.Lone'),
            'interior-target':SOURCE.replace('.long .Lzero,.Lone','.long .Lzero+1,.Lone'),
            'missing-entry':SOURCE.replace('.long .Lzero,.Lone','.long .Lzero'),
            'unbalanced-stack':SOURCE.replace(' pop %edi\n',''),
            'index-clobber':SOURCE.replace(' inc %edi',' mov %eax,%edi\n inc %edi'),
            'flag-clobber':SOURCE.replace(' jbe .Ldispatch',' test %eax,%eax\n jbe .Ldispatch'),
            'frame-escape':SOURCE.replace(' inc %edi',' lea (%esp),%eax\n inc %edi'),
            'unknown-call':SOURCE.replace(' inc %edi',' call *%eax\n inc %edi'),
            'named-interior':SOURCE.replace('.Ladvance:\n','.globl bypass\nbypass:\n.Ladvance:\n'),
            'return-overwrite':SOURCE.replace(' inc %edi',' movl $0,4(%esp)\n inc %edi'),
            'saved-index-overwrite':SOURCE.replace(' inc %edi',' movl $0,(%esp)\n inc %edi'),
            'partial-stack-write':SOURCE.replace(' inc %edi',' mov $0,%sp\n inc %edi'),
            'segment-write':SOURCE.replace(' inc %edi',' mov %ax,%ss\n inc %edi'),
            'word-push':SOURCE.replace(' push %edi',' push %di').replace(' pop %edi',' add $4,%esp'),
            'foreign-interior-reloc':SOURCE+'.data\n.long .text@GOTOFF+(.Ladvance-fixture)\n',
            'wrong-restore':SOURCE.replace(' pop %edi',' pop %esi'),
            'duplicate-call-reloc':SOURCE.replace(' inc %edi','.Lcall:\n call external\n inc %edi')+'.type external,@function\n.reloc .Lcall+1,R_386_PC32,external\n',
        }
        for label,source in forms.items():
            asm=folder/(label+'.s');obj=folder/(label+'.o');asm.write_text(source)
            subprocess.run(['as','--32','-o',str(obj),str(asm)],check=True)
            try:
                proof=p.prove(str(obj),'fixture')
            except j.Refused as exc:
                assert label not in ('positive','changed-target'),(label,str(exc))
                report[label]={'refused':str(exc)}
            else:
                assert label in ('positive','changed-target'),label
                report[label]=proof
        assert report['positive']['identity']!=report['changed-target']['identity']
        assert report['positive']['induction']['dispatch_values']==[0,1]
        # Stack-slot home changes are outside this table proof and never erased.
    print(json.dumps(report,indent=2))
    print(len(report),'ELF controls: 2 proved (different identities),',len(report)-2,'refused')


if __name__=='__main__':
    main()
