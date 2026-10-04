#!/usr/bin/env python3
"""Audit the complete DFT TUs and input/channel lifetime controls."""
import json
import re
import playbook_small_patterns as driver
from gcc3_reload_trace import instructions
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection

b=driver.b
source=(driver.ROOT/'tools/playbook_bwch_unit_audit.py').read_text()
exec(source[source.index('def inspect'):source.index('reports=')])
reports={};cells=[]
for suffix in ['', '-channel']:
    root=driver.ROOT/'build'/('gcc3-v34-dft-boundaries'+suffix)
    ledger=json.loads((root/'results.json').read_text())
    family=ledger['families']['DFTC']
    base=root/'DFTC/baseline/candidate.o'
    baseline=inspect(base)
    assert len(b.sizes(str(base)))==3 and len(baseline['objects'])==1
    for label,cell in family['cells'].items():
        path=root/'DFTC'/label/'candidate.o'
        report=inspect(path)
        assert report==baseline,(suffix,label,'metadata/data')
        with path.open('rb') as stream:
            elf=ELFFile(stream)
            nontext={s.name:s.data().hex() for s in elf.iter_sections() if s['sh_flags']&2 and s.name!='.text'}
            for relsec in elf.iter_sections():
                if isinstance(relsec,RelocationSection):
                    assert elf.get_section(relsec['sh_info']).name=='.text'
            symtab=elf.get_section_by_name('.symtab')
            relocations=[]
            for relsec in elf.iter_sections():
                if not isinstance(relsec,RelocationSection):
                    continue
                raw=elf.get_section(relsec['sh_info']).data()
                for relocation in relsec.iter_relocations():
                    offset=relocation['r_offset'];symbol=symtab.get_symbol(relocation['r_info_sym'])
                    assert relocation['r_info_type']==1 and symbol.name=='costbl'
                    relocations.append(('R_386_32',b.relocation_target('R_386_32',symbol.name,raw[offset:offset+4],b.section_symbols(str(path)))))
            if label=='baseline':baseline_relocations=relocations
            assert relocations==baseline_relocations and len(relocations)==3
        if label=='baseline':baseline_nontext=nontext
        assert nontext==baseline_nontext
        changed=[n for n in b.sizes(str(base)) if b.body(str(path),n)!=b.body(str(base),n)]
        assert set(changed)<={'dftupdate'},(suffix,label,changed)
        assert not cell.get('losses',[])
        report.update(nontext=nontext,changed_bodies=changed,canonical_relocation_targets=relocations)
        reports[suffix+'/'+label]=report
        cells.append(cell)
assert len(cells)==12
assert sum(c['verdicts']['dftupdate'][0]=='EXACT' for c in cells)==1
winner=driver.ROOT/'build/gcc3-v34-dft-boundaries-channel/DFTC/cursor-per-bin-channel/candidate.o'
assert b.verdict(*b.body(b.BLOB,'dftupdate'),*b.body(str(winner),'dftupdate'))==('EXACT',0)


def nodes(pattern):
    if isinstance(pattern,list):
        yield pattern
        for child in pattern[1:]:
            yield from nodes(child)


stages={}
for label in ['baseline','channel-only','cursor-per-bin-interleaved','cursor-per-bin-channel']:
    folder=driver.ROOT/'build/gcc3-v34-dft-boundaries-channel/DFTC'/label
    for stage in ['01.rtl','20.combine','22.regmove','24.lreg','25.greg','33.sched2']:
        text=(folder/('DFTC.c.'+stage)).read_text()
        found=[s for s in re.split(r'^;; Function ',text,flags=re.M)[1:] if s.splitlines()[0].strip()=='dftupdate']
        assert len(found)==1,(label,stage)
        selected=';; Function '+found[0]
        (folder/('dftupdate.'+stage)).write_text(selected)
        stages[label+'/'+stage]=instructions(selected)


def one(patterns,predicate):
    hits=[(uid,node) for uid,pattern in patterns.items() for node in nodes(pattern) if predicate(node)]
    assert len(hits)==1,hits
    return hits[0]


for label,channel,cursor,inside in [('baseline',False,False,False),('channel-only',True,False,False),('cursor-per-bin-interleaved',False,True,True),('cursor-per-bin-channel',True,True,True)]:
    patterns=stages[label+'/20.combine']
    multiply,_=one(patterns,lambda n:n[0]=='set' and 'im' in n[1] and n[2][0]=='mult:SI')
    convert,_=one(patterns,lambda n:n[0]=='set' and n[2][0]=='float:DF' and 're' in n[2][1])
    order=list(patterns)
    assert (order.index(convert)<order.index(multiply))==channel
    load_uid,load=one(patterns,lambda n:n[0]=='set' and 'x' in n[1] and n[2][0]=='sign_extend:SI' and n[2][1][0].startswith('mem'))
    address=load[2][1][1]
    assert address[0]==('reg/v/f:SI' if cursor else 'plus:SI')
    phase_uid,_=one(patterns,lambda n:n[0]=='set' and n[1][0]=='mem/s:HI' and n[1][1][0].startswith('reg') and 'b' in n[1][1])
    assert (order.index(load_uid)>order.index(phase_uid))==inside
summary={'valid_cells':12,'source_hashes':len({c['source_hash'] for c in cells}),'object_hashes':len({c['object_hash'] for c in cells}),
         'full_tus':reports,'functions_per_tu':3,'data_per_tu':1,'stage_records':len(stages),'boundary_controls':12}
(driver.ROOT/'build/dft-full-tu-audit.json').write_text(json.dumps(summary,indent=2)+'\n')
(driver.ROOT/'build/dft-stage-patterns.json').write_text(json.dumps(stages,indent=2)+'\n')
print('12/12 complete TUs: 3 functions/1 data, metadata/table/nontext/relocations/bystanders agree; 1 exact candidate')
print('24/24 selected stage records; 12/12 positive/negative channel/input-scope controls pass')
print('12 cells /',summary['source_hashes'],'sources /',summary['object_hashes'],'raw objects')
