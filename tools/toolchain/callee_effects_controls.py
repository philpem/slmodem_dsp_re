#!/usr/bin/env python3
"""Gentoo assembly controls for machine-derived leaf effects and call binding."""
import argparse
import json
import subprocess
from pathlib import Path
import callee_effects as c

ROOT = Path(__file__).resolve().parents[2]
IMAGE = 'ghcr.io/philpem/gcc-3.4.2-gentoo2005-docker:latest'
SOURCE = '''.text
.globl sample
.type sample,@function
sample:
mov 8(%esp),%ecx
mov 4(%esp),%eax
movzbl (%eax),%edx
mov %dl,(%ecx)
movzbl 1(%ecx),%edx
and $0xfe,%dl
or $1,%dl
mov %dl,1(%ecx)
mov $1,%eax
ret
.size sample,.-sample
.globl caller
.type caller,@function
caller:
call sample
ret
.size caller,.-caller
.section .note.GNU-stack,"",@progbits
'''


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--out',type=Path,default=ROOT/'scratch/callee-effects/controls')
    args=ap.parse_args(); out=args.out.resolve(); out.mkdir(parents=True,exist_ok=True)
    relative=out.relative_to(ROOT)
    positive={
        'baseline':SOURCE,
        'word-mask':SOURCE.replace('and $0xfe,%dl','and $0xfffe,%dx'),
        'accumulator-and16':SOURCE.replace('movzbl 1(%ecx),%edx\nand $0xfe,%dl\nor $1,%dl\nmov %dl,1(%ecx)', 'movzbl 1(%ecx),%eax\nand $0xfffe,%ax\nor $1,%al\nmov %al,1(%ecx)'),
        'accumulator-and32':SOURCE.replace('mov $1,%eax','mov $128,%eax\nand $0x80,%eax\nmov $1,%eax'),
        'unreachable-padding':SOURCE.replace('.size sample,.-sample','lea 0(%esi),%esi\n.size sample,.-sample'),
        'rebased':SOURCE.replace('.text\n','.text\n.space 16,0x90\n',1),
        'two-paths':SOURCE.replace('mov %dl,(%ecx)', 'test %edx,%edx\nje .Lother\nmov %dl,(%ecx)\njmp .Ljoin\n.Lother:\nmovb $2,(%ecx)\n.Ljoin:'),
        'saved-ebx':SOURCE.replace('sample:\nmov 8(%esp),%ecx\nmov 4(%esp),%eax', 'sample:\npush %ebx\nmov 12(%esp),%ecx\nmov 8(%esp),%eax').replace('mov $1,%eax\nret', 'mov $3,%ebx\nmov $1,%eax\npop %ebx\nret'),
        'high-byte':SOURCE.replace('and $0xfe,%dl\nor $1,%dl', 'mov %dl,%dh\nand $0xfe,%dh\nor $1,%dh\nmov %dh,%dl'),
        'setcc':SOURCE.replace('or $1,%dl', 'test %eax,%eax\nsete %al\nor %al,%dl'),
    }
    negative={
        'relocated-output-argument':SOURCE.replace('mov 8(%esp),%ecx','.byte 0x8b,0x8c,0x24\n.long anchor+8')+'\n.section .rodata\nanchor: .long 0\n',
        'relocated-record-read':SOURCE.replace('movzbl 1(%ecx),%edx','.byte 0x0f,0xb6,0x91\n.long anchor+1')+'\n.section .rodata\nanchor: .long 0\n',
        'prefixed-lea':SOURCE.replace('mov $1,%eax','.byte 0x66\nlea 0(%esi),%esi\nmov $1,%eax'),
        'relocated-lea':SOURCE.replace('mov $1,%eax','lea padding(%esi),%esi\nmov $1,%eax')+'\n.section .rodata\npadding: .long 0\n',
        'nonidentity-lea':SOURCE.replace('mov $1,%eax','lea 1(%esi),%esi\nmov $1,%eax'),
        'fallthrough-padding':SOURCE.replace('mov $1,%eax\nret','mov $1,%eax\nlea 0(%esi),%esi'),
        'unreachable-write':SOURCE.replace('.size sample,.-sample','movb $0,(%ecx)\n.size sample,.-sample'),
        'record-derived-read-address':SOURCE.replace('mov 4(%esp),%eax','mov (%ecx),%eax\nmov (%eax),%edx\nand $0,%edx\nmov 4(%esp),%eax'),
        'missing-write':SOURCE.replace('mov %dl,1(%ecx)','nop'),
        'missing-one-path-write':positive['two-paths'].replace('movb $2,(%ecx)','nop'),
        'partial-mask':SOURCE.replace('and $0xfe,%dl\nor $1,%dl','and $0xfd,%dl\nor $2,%dl'),
        'extra-call':SOURCE.replace('mov $1,%eax','call other\nmov $1,%eax'),
        'partial-pointer':SOURCE.replace('mov $1,%eax','mov $0,%cl\nmov $1,%eax'),
        'pointer-mutation':SOURCE.replace('mov $1,%eax','add $1,%ecx\nmov $1,%eax'),
        'pointer-copy':SOURCE.replace('mov $1,%eax','mov %ecx,%edx\nmov $1,%eax'),
        'pointer-store':SOURCE.replace('mov $1,%eax','mov %ecx,(%ecx)\nmov $1,%eax'),
        'pointer-return':SOURCE.replace('mov $1,%eax','mov %ecx,%eax'),
        'out-of-capacity':SOURCE.replace('mov %dl,1(%ecx)','mov %dl,8(%ecx)'),
        'negative-offset':SOURCE.replace('mov %dl,1(%ecx)','mov %dl,-1(%ecx)'),
        'indexed-record':SOURCE.replace('mov %dl,1(%ecx)','mov %dl,1(%ecx,%eax)'),
        'external-write':SOURCE.replace('mov %dl,1(%ecx)','mov %dl,1(%eax)'),
        'implicit-write':SOURCE.replace('mov $1,%eax','stosb\nmov $1,%eax'),
        'implicit-registers':SOURCE.replace('mov $1,%eax','cpuid\nmov $1,%eax'),
        'prefix':SOURCE.replace('mov %dl,1(%ecx)','.byte 0xf0\nmov %dl,1(%ecx)'),
        'unsaved-register':SOURCE.replace('mov $1,%eax','mov $1,%ebx\nmov $1,%eax'),
        'unrestored-register':positive['saved-ebx'].replace('pop %ebx','nop'),
        'save-overlap':positive['saved-ebx'].replace('mov 8(%esp),%eax','mov (%esp),%eax'),
        'return-word':SOURCE.replace('mov 4(%esp),%eax','mov (%esp),%eax'),
        'far-argument':SOURCE.replace('mov 4(%esp),%eax','mov 44(%esp),%eax'),
        'incoming-register':SOURCE.replace('mov 4(%esp),%eax','nop'),
        'double-argument-load':SOURCE.replace('mov $1,%eax','mov 8(%esp),%edx\nmov $1,%eax'),
        'return-adjust':SOURCE.replace('\nret\n','\nret $4\n',1),
        'loop':SOURCE.replace('mov $1,%eax','.Lloop:\njmp .Lloop\nmov $1,%eax'),
        'interior-entry':SOURCE.replace('movzbl 1(%ecx),%edx','.globl interior\ninterior:\nmovzbl 1(%ecx),%edx'),
        'external-interior-branch':SOURCE.replace('movzbl 1(%ecx),%edx','.Linterior:\nmovzbl 1(%ecx),%edx').replace('caller:\ncall sample','caller:\njmp .Linterior\ncall sample'),
        'record-dependent-branch':SOURCE.replace('and $0xfe,%dl\nor $1,%dl','test %edx,%edx\nje .Lzero\nmov $1,%edx\njmp .Ljoin\n.Lzero:\nmov $0,%edx\n.Ljoin:'),
        'undefined-flags':SOURCE.replace('mov 4(%esp),%eax','sete %al\nmov 4(%esp),%eax'),
        'zero-shift-flags':SOURCE.replace('and $0xfe,%dl','shl $0,%dl\nand $0xfe,%dl'),
        'weak-definition':SOURCE.replace('.globl sample','.weak sample'),
        'alias-definition':SOURCE.replace('sample:\n','sample:\n.globl alias\n.type alias,@function\nalias:\n'),
        'wrong-call-addend':SOURCE.replace('call sample','call sample+1'),
        'wrong-call-target':SOURCE.replace('call sample','call other'),
        'wrong-call-kind':SOURCE.replace('call sample','call *%eax'),
        'undefined-target':SOURCE.replace('.globl sample\n.type sample,@function\nsample:', '.globl implementation\n.type implementation,@function\nimplementation:').replace('.size sample,.-sample','.size implementation,.-implementation'),
    }
    all_sources={**positive,**negative}
    for label,source in all_sources.items(): (out/(label+'.s')).write_text(source)
    script='set -eu\nas --version\n'+'\n'.join(
        f'as --32 -o /work/{relative}/{label}.o /work/{relative}/{label}.s' for label in all_sources)
    command=['docker','run','--rm','-v',str(ROOT)+':/work',IMAGE,'/bin/sh','-c',script]
    assembled=subprocess.run(command,check=True,capture_output=True,text=True)
    results={}
    for label in all_sources:
        path=str(out/(label+'.o'))
        try:
            _,_,_,caller,_,_,_=c.artifact(path,'caller',check_entry=False)
            proof=c.prove_call(path,'caller',caller['st_value'],1,8)
            c.require_initialized(proof,0,255); c.require_initialized(proof,1,1)
            accepted=True; reason=None
        except c.Refused as error:
            accepted=False; reason=str(error); proof=None
        assert accepted == (label in positive),(label,accepted,reason)
        results[label]={'accepted':accepted,'reason':reason,'proof':proof}
    cache_controls={}
    for label,replacement in [('changed-body','partial-mask'),
                              ('changed-external-entry','external-interior-branch')]:
        path=out/('cache-'+label+'.o')
        path.write_bytes((out/'baseline.o').read_bytes())
        c.prove(str(path),'sample',1,8)
        path.write_bytes((out/(replacement+'.o')).read_bytes())
        try: c.prove(str(path),'sample',1,8)
        except c.Refused as error: cache_controls[label]=str(error)
        else: raise AssertionError(('stale cached proof accepted',label))
    native=c.prove_call(str(ROOT/'ref/slmodemd/dsplibs.o'),'v22_local_loop',0x8a94f,1,44)
    assert native['instructions']==94 and native['paths']==4 and native['store_instructions']==17
    assert native['record_write_masks']=={str(x):255 for x in list(range(12))+list(range(16,22))}
    c.require_initialized(native,21,1)
    try: c.require_initialized(native,21,2)
    except c.Refused: pass
    else: raise AssertionError('native preserved bit incorrectly declared initialized')
    payload={'controls':results,'cache_controls':cache_controls,'native':native,'command':command,'assembler':assembled.stdout,
             'positive':len(positive),'negative':len(negative)}
    (out/'results.json').write_text(json.dumps(payload,indent=2)+'\n')
    print(f'{len(positive)} positive leaf/call controls; {len(negative)} adversarial refusals; native 94 instructions / 17 stores / 4 paths; preserved-bit and 2 stale-cache negatives passed')


if __name__=='__main__': main()
