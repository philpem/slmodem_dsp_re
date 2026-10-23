#!/usr/bin/env python3
"""Gentoo assembly controls for machine-derived leaf effects and call binding.

Run from a tree with the blob, the period objects and the recovered-order
partial link (`make tc && make partial-link`): the last controls prove the real
`v22_local_loop` caller on the per-TU object through the link, and keep the
blob's own defined callee on its per-TU path.
"""
import argparse
import hashlib
import io
import json
import subprocess
from pathlib import Path
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
import callee_effects as c
import jumptable as j

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
    callee=SOURCE[:SOURCE.index('.globl caller')]
    caller_only=('.text\n.globl caller\n.type caller,@function\ncaller:\n'
                 'call sample\nret\n.size caller,.-caller\n'
                 '.section .note.GNU-stack,"",@progbits\n')
    rel_callee=(callee.replace(
        'movzbl 1(%ecx),%edx',
        'mov table,%edx\nmov table+4,%edx\nmovzbl 1(%ecx),%edx')
        +'.section .rodata\ntable: .long 0\n.long 0\n')
    rodata_pad=('.section .rodata\n.space 16\n.data\n.globl pad_marker\n'
                'pad_marker: .long 0\n')
    sym_callee=callee.replace('movzbl 1(%ecx),%edx',
                              'mov exttable,%edx\nmovzbl 1(%ecx),%edx')
    extdata=('.section .rodata\n.globl exttable\n.type exttable,@object\n'
             'exttable: .long 0\n.size exttable,.-exttable\n'
             '.data\n.globl ext_marker\next_marker: .long 0\n')
    cross_sources={
        'x-good-callee':callee,
        'x-bad-callee':callee.replace('mov %dl,1(%ecx)','mov %dl,8(%ecx)'),
        'x-caller':caller_only,
        'x-defined-caller':callee+caller_only,
        'x-rel-callee':rel_callee,
        'x-rodata-pad':rodata_pad,
        'x-sym-callee':sym_callee,
        'x-extdata':extdata,
    }
    for label,source in all_sources.items(): (out/(label+'.s')).write_text(source)
    for label,source in cross_sources.items(): (out/(label+'.s')).write_text(source)
    commands=[f'as --32 -o /work/{relative}/{label}.o /work/{relative}/{label}.s'
              for label in list(all_sources)+list(cross_sources)]
    commands += [
        f'ld -r -o /work/{relative}/x-link-good.o /work/{relative}/x-good-callee.o /work/{relative}/x-caller.o',
        f'ld -r -o /work/{relative}/x-link-caller-only.o /work/{relative}/x-caller.o',
        f'ld -r -o /work/{relative}/x-link-bad.o /work/{relative}/x-bad-callee.o /work/{relative}/x-caller.o',
        f'ld -r -o /work/{relative}/x-link-rel.o /work/{relative}/x-rodata-pad.o /work/{relative}/x-rel-callee.o /work/{relative}/x-caller.o',
        f'ld -r -o /work/{relative}/x-link-rel-nopad.o /work/{relative}/x-rel-callee.o /work/{relative}/x-caller.o',
        f'ld -r -o /work/{relative}/x-link-sym.o /work/{relative}/x-extdata.o /work/{relative}/x-sym-callee.o /work/{relative}/x-caller.o',
    ]
    script='set -eu\nas --version\n'+'\n'.join(commands)
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
    def digest(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
    def call_site(path):
        _,_,_,fn,_,_,_=c.artifact(str(path),'caller',check_entry=False)
        return fn['st_value']
    def section_data(path,name):
        with open(path,'rb') as stream:
            return ELFFile(stream).get_section_by_name(name).data()
    def patch_section_addends(source,target,addends):
        data=bytearray(Path(source).read_bytes())
        elf=ELFFile(io.BytesIO(data))
        text=elf.get_section_by_name('.text')
        fields=[]
        for section in elf.iter_sections():
            if (isinstance(section,RelocationSection) and
                    section['sh_info']==elf.get_section_index('.text')):
                symbols=elf.get_section(section['sh_link'])
                for relocation in section.iter_relocations():
                    target_symbol=symbols.get_symbol(relocation['r_info_sym'])
                    if (target_symbol['st_info']['type']=='STT_SECTION' and
                            elf.get_section(target_symbol['st_shndx']).name=='.rodata'):
                        fields.append(relocation['r_offset'])
        assert len(fields)==len(addends),(fields,addends)
        for field,addend in zip(sorted(fields),addends):
            offset=text['sh_offset']+field
            data[offset:offset+4]=addend.to_bytes(4,'little')
        Path(target).write_bytes(bytes(data))
    def pre_placement_acceptance(source,linked,name):
        """The pre-change check: body, shape and a per-target uniform delta."""
        source_raw,source_relocations=c._function_identity(source,name)
        linked_raw,linked_relocations=c._function_identity(linked,name)
        if (len(source_raw)!=len(linked_raw) or
                [r[:3] for r in source_relocations]!=[r[:3] for r in linked_relocations] or
                source_raw!=linked_raw):
            return False
        rebase={}
        for source_record,linked_record in zip(source_relocations,linked_relocations):
            if (source_record[1],source_record[2])!=(linked_record[1],linked_record[2]):
                return False
            delta=linked_record[3]-source_record[3]
            if rebase.setdefault((source_record[1],source_record[2]),delta)!=delta:
                return False
        return True
    cross_controls={}
    def cross_case(label,caller,linked,objects,capacity=8,expect=True):
        manifest=out/(label+'.manifest')
        manifest.write_text(''.join(f'{obj} {obj[:-2]}.s\n' for obj in objects))
        try:
            proof=c.prove_call(str(out/caller),'caller',call_site(out/caller),1,capacity,
                               link=str(out/linked),manifest=str(manifest))
            accepted=True; reason=None
        except c.Refused as error:
            accepted=False; reason=str(error); proof=None
        assert accepted==expect,(label,accepted,reason)
        cross_controls[label]={'accepted':accepted,'reason':reason,'proof':proof}
        return proof
    linked_positive=cross_case('linked-positive','x-caller.o','x-link-good.o',
                               ['x-good-callee.o','x-caller.o'])
    assert linked_positive['call']['resolution']=='recovered-order-link'
    assert linked_positive['artifact_sha256']==digest(out/'x-link-good.o')
    assert linked_positive['binding']=='STB_GLOBAL'
    cross_case('linked-missing','x-caller.o','x-link-caller-only.o',['x-caller.o'],expect=False)
    cross_case('linked-capacity','x-caller.o','x-link-bad.o',
               ['x-bad-callee.o','x-caller.o'],expect=False)
    assert 'capacity' in cross_controls['linked-capacity']['reason']
    cross_case('linked-mismatch','x-caller.o','x-link-bad.o',
               ['x-good-callee.o','x-caller.o'],expect=False)
    assert 'manifest-selected copy' in cross_controls['linked-mismatch']['reason']
    local=cross_case('per-object-defined','x-defined-caller.o','x-link-bad.o',
                     ['x-bad-callee.o','x-caller.o'])
    assert local['call']['resolution']=='per-tu'
    assert local['artifact_sha256']==digest(out/'x-defined-caller.o')
    rel_objects=['x-rodata-pad.o','x-rel-callee.o','x-caller.o']
    rebased=cross_case('linked-rebased','x-caller.o','x-link-rel.o',rel_objects)
    assert rebased['call']['resolution']=='recovered-order-link'
    assert rebased['call']['callee_rebase']=={
        'section_rebases':{'.rodata':{'placement':16,
                                      'observed_addend_delta':16}},
        'symbol_rebases':{}}
    assert (section_data(out/'x-link-rel.o','.rodata')[16:24]==
            section_data(out/'x-rel-callee.o','.rodata'))
    sym_objects=['x-extdata.o','x-sym-callee.o','x-caller.o']
    resolved=cross_case('linked-symbol-resolution','x-caller.o','x-link-sym.o',
                        sym_objects)
    symbol_rebase=resolved['call']['callee_rebase']
    assert symbol_rebase['section_rebases']=={}
    assert symbol_rebase['symbol_rebases']['exttable']=={
        'section':None,'resolved_section':'.rodata',
        'observed_value_delta':None,'observed_addend_delta':0}
    patch_section_addends(out/'x-link-rel.o',out/'x-link-rel-changed.o',[0x14,0x18])
    patch_section_addends(out/'x-link-rel.o',out/'x-link-rel-stale.o',[0x20,0x24])
    for label in ('x-link-rel-changed.o','x-link-rel-stale.o'):
        assert pre_placement_acceptance(out/'x-rel-callee.o',out/label,'sample'),label
    cross_case('linked-changed-addend','x-caller.o','x-link-rel-changed.o',
               rel_objects,expect=False)
    assert 'section placement' in cross_controls['linked-changed-addend']['reason']
    cross_case('linked-stale-uniform','x-caller.o','x-link-rel-stale.o',
               rel_objects,expect=False)
    assert 'section placement' in cross_controls['linked-stale-uniform']['reason']
    assert pre_placement_acceptance(out/'x-rel-callee.o',out/'x-link-rel-nopad.o','sample')
    cross_case('linked-missing-layout','x-caller.o','x-link-rel-nopad.o',
               rel_objects,expect=False)
    assert ('does not explain linked section' in
            cross_controls['linked-missing-layout']['reason'])
    missing_section={}
    try:
        with open(out/'x-link-rel.o','rb') as stream:
            c._section_placement(out/'linked-rebased.manifest',out/'x-rel-callee.o',
                                 '.absent',ELFFile(stream))
    except c.Refused as error:
        missing_section['absent-target-section']=str(error)
    else:
        raise AssertionError('missing target section accepted')
    assert 'no target section' in missing_section['absent-target-section']
    native=c.prove_call(str(ROOT/'ref/slmodemd/dsplibs.o'),'v22_local_loop',0x8a94f,1,44)
    assert native['call']['resolution']=='per-tu'
    assert native['instructions']==94 and native['paths']==4 and native['store_instructions']==17
    assert native['record_write_masks']=={str(x):255 for x in list(range(12))+list(range(16,22))}
    c.require_initialized(native,21,1)
    try: c.require_initialized(native,21,2)
    except c.Refused: pass
    else: raise AssertionError('native preserved bit incorrectly declared initialized')
    tc=ROOT/'build/tc_out/src_pump_v22_v22mod.c.o'
    linked=ROOT/'build/partial/dsplibs.o'
    manifest=ROOT/'build/tc_repro/tc_link_manifest.txt'
    if not (tc.is_file() and linked.is_file() and manifest.is_file()):
        raise AssertionError('real v22 cross-TU control needs build/tc_out, '
                             'build/partial/dsplibs.o and build/tc_repro/'
                             'tc_link_manifest.txt; run make tc && make partial-link')
    reconstructed=c.prove_call(str(tc),'v22_local_loop',0x21bf,1,44,
                               link=str(linked),manifest=str(manifest))
    assert reconstructed['call']['resolution']=='recovered-order-link'
    assert reconstructed['size']==315 and reconstructed['paths']==32
    assert reconstructed['store_instructions']==17
    assert reconstructed['artifact_sha256']==digest(linked)
    assert reconstructed['call']['callee_rebase']=={
        'section_rebases':{'.rodata':{'placement':24948,
                                      'observed_addend_delta':24948}},
        'symbol_rebases':{}}
    selected=Path(reconstructed['call']['selected_object'])
    source_rodata=section_data(selected,'.rodata')
    linked_rodata=section_data(linked,'.rodata')
    assert linked_rodata[24948:24948+len(source_rodata)]==source_rodata
    framed=j.prove(str(tc),'v22_local_loop',link=str(linked),manifest=str(manifest))
    record=framed['fixed_stack_frame']['private_record']
    assert framed['count']==14 and record['capacity']==44
    assert record['callee']['size']==315 and record['callee']['paths']==32
    payload={'controls':results,'cache_controls':cache_controls,
             'cross_tu_controls':cross_controls,'native':native,
             'reconstructed_v22':{'callee':reconstructed,'frame':framed},
             'command':command,'assembler':assembled.stdout,
             'positive':len(positive),'negative':len(negative),
             'cross_positive':3,'cross_negative':6,'cross_per_object':1,
             'missing_section_controls':missing_section}
    (out/'results.json').write_text(json.dumps(payload,indent=2)+'\n')
    print(f'{len(positive)} positive leaf/call controls; {len(negative)} adversarial refusals; '
          f'10 cross-TU controls (3 positive, 6 refusals, 1 per-object) plus '
          f'1 missing-target-section refusal; '
          f'native 94 instructions / 17 stores / 4 paths; reconstructed 315-byte callee / '
          f'32 paths / 44-byte envelope; preserved-bit and 2 stale-cache negatives passed')


if __name__=='__main__': main()
