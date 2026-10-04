#!/usr/bin/env python3
"""Audit complete V.90 small-boundary controls and float2Bits RTL origins."""
import json,re
from pathlib import Path
import playbook_small_patterns as driver
from gcc3_reload_trace import instructions
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
b=driver.b
text=(driver.ROOT/'tools/playbook_bwch_unit_audit.py').read_text()
exec(text[text.index('def inspect'):text.index('reports=')])
def nontext(path):
    result={}
    with path.open('rb') as stream:
        elf=ELFFile(stream);syms=elf.get_section_by_name('.symtab');names={i:sec.name for i,sec in enumerate(elf.iter_sections())}
        for i,section in enumerate(elf.iter_sections()):
            if not section['sh_flags']&2 or section.name=='.text':continue
            raw=bytearray(section.data());relocations={}
            for relsec in elf.iter_sections():
                if not isinstance(relsec,RelocationSection) or relsec['sh_info']!=i:continue
                for r in relsec.iter_relocations():
                    offset=r['r_offset'];symbol=syms.get_symbol(r['r_info_sym']);kind={1:'R_386_32',2:'R_386_PC32'}[r['r_info_type']]
                    target=b.relocation_target(kind,symbol.name or names[symbol['st_shndx']],bytes(raw[offset:offset+4]),b.section_symbols(str(path)))
                    if target[:2]==('section','.text'):
                        owners=[sym for sym in syms.iter_symbols() if sym['st_info']['type']=='STT_FUNC' and sym['st_shndx']==elf.get_section_index('.text') and sym['st_value']<=target[2]<sym['st_value']+sym['st_size']]
                        assert len(owners)==1,(path,target)
                        owner=owners[0];target=('function',owner.name,target[2]-owner['st_value'])
                    relocations[offset]=(kind,target);raw[offset:offset+4]=b'\0'*4
            if section.name in ('.rodata.cst4','.rodata.cst8'):
                width=int(section.name[-1]);assert len(raw)%width==0 and not relocations
                values=sorted(bytes(raw[k:k+width]).hex() for k in range(0,len(raw),width))
                result[section.name]={'constant_pool_values':values,'relocations':relocations}
            else:result[section.name]={'bytes':raw.hex(),'relocations':relocations}
    return result

reports={};total=0
for suffix in ('small','cppck','parameters','adid'):
    root=driver.ROOT/'build'/('gcc3-batch20-v90-'+suffix)
    ledger=json.loads((root/'results.json').read_text())
    for family,rec in ledger['families'].items():
        base=root/family/'baseline/candidate.o';meta=inspect(base);data=nontext(base)
        expected={'V90DilDescriptorSettings':4,'V90ModulusDecoder':5,'V90CPpck':2,'V90Parameters':9,'V90AutoDigitalImpDetector':34}[family]
        assert len(b.sizes(str(base)))==expected
        for label,cell in rec['cells'].items():
            obj=root/family/label/'candidate.o'
            assert inspect(obj)==meta,(family,label,'metadata/data')
            assert nontext(obj)==data,(family,label,'nontext')
            assert not cell.get('losses'),(family,label)
            if family=='V90CPpck':assert set(cell.get('changed_bodies',[]))<={'_Z10float2BitsfPsi'}
            reports[family+'/'+label]={'changed_bodies':cell.get('changed_bodies',[]),'gains':cell.get('gains',[]),'functions':expected,'named_data':len(meta['objects'])}
            total+=1
assert total==24
root=driver.ROOT/'build/gcc3-batch20-v90-cppck/V90CPpck'
records={}
for label in ('baseline','switch','subtraction-first','switch-subtraction-first'):
    for stage in ('01.rtl','04.jump','20.combine','25.greg','31.bbro','33.sched2'):
        text=(root/label/('V90CPpck.cpp.'+stage)).read_text()
        bodies=[s for s in re.split(r'^;; Function ',text,flags=re.M)[1:] if s.startswith('void float2Bits(float, short int*, int)')]
        assert len(bodies)==1,(label,stage)
        records[label+'/'+stage]=instructions(bodies[0])
def nodes(p):
    if isinstance(p,list):
        yield p
        for child in p[1:]:yield from nodes(child)
for label in ('baseline','switch','subtraction-first','switch-subtraction-first'):
    patterns=records[label+'/01.rtl']
    branches=[n for p in patterns.values() for n in nodes(p) if n[0]=='if_then_else']
    assert branches[0][1][0]==('eq' if label.startswith('switch') else 'ne')
    tablebranches=[n for n in branches if "'x'" in str(n[1])]
    assert len(tablebranches)==2
    assert [n[1][0] for n in tablebranches]==(['le','le'] if 'subtraction-first' in label else ['gt','gt'])
winner=root/'switch-subtraction-first/candidate.o'
assert b.verdict(*b.body(b.BLOB,'_Z10float2BitsfPsi'),*b.body(str(winner),'_Z10float2BitsfPsi'))==('EXACT',0)
(driver.ROOT/'build/batch20-v90-small-full-tu-audit.json').write_text(json.dumps({'valid_cells':total,'reports':reports,'stage_records':len(records),'causal_controls':8},indent=2)+'\n')
(driver.ROOT/'build/batch20-v90-small-stage-patterns.json').write_text(json.dumps(records,indent=2)+'\n')
print('24/24 completeTU metadata/data/nontext relocation/bystander controls;24/24 float2Bits stage records; exact283B')
