#!/usr/bin/env python3
"""Audit complete root-owned batch20 controls, including original string additions."""
import json
from collections import Counter
from pathlib import Path
import playbook_small_patterns as driver
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
b=driver.b
source=(driver.ROOT/'tools/playbook_bwch_unit_audit.py').read_text()
exec(source[source.index('def inspect'):source.index('reports=')])

def allocated(path):
    result={}
    with path.open('rb') as stream:
        elf=ELFFile(stream);tab=elf.get_section_by_name('.symtab')
        names={i:s.name for i,s in enumerate(elf.iter_sections())}
        for i,section in enumerate(elf.iter_sections()):
            if not section['sh_flags']&2 or section.name=='.text':continue
            raw=bytearray(section.data());relocs={}
            for rsec in elf.iter_sections():
                if not isinstance(rsec,RelocationSection) or rsec['sh_info']!=i:continue
                for rel in rsec.iter_relocations():
                    off=rel['r_offset'];sym=tab.get_symbol(rel['r_info_sym'])
                    kind={1:'R_386_32',2:'R_386_PC32'}[rel['r_info_type']]
                    target=b.relocation_target(kind,sym.name or names[sym['st_shndx']],bytes(raw[off:off+4]),b.section_symbols(str(path)))
                    if target[:2]==('section','.text'):
                        address=target[2]
                        owners=[s for s in tab.iter_symbols() if s['st_info']['type']=='STT_FUNC' and s['st_shndx']==elf.get_section_index('.text') and s['st_value']<=address<s['st_value']+s['st_size']]
                        assert len(owners)==1,(path,address)
                        target=('function',owners[0].name,address-owners[0]['st_value'])
                    relocs[off]=(kind,target);raw[off:off+4]=b'\0'*4
            result[section.name]={'bytes':raw.hex(),'relocations':relocs}
    return result

known={'call: delete...\n','call: create...\n','call: create RC: %d <-> %d...\n','call: process: msg %d --> %d\n'}
with open(b.BLOB,'rb') as stream:
    elf=ELFFile(stream)
    pool=b'\0'.join(s.data() for s in elf.iter_sections() if s.name.startswith('.rodata.str'))
    assert all(s.encode()+b'\0' in pool for s in known)
reports={};total=0
for family in ('psd','call-delete','call-processing','call-create','call-queue','call-dial'):
    root=driver.ROOT/'build'/('gcc3-batch20-'+family)
    ledger=json.loads((root/'results.json').read_text())
    for name,data in ledger['families'].items():
        basepath=root/name/'baseline/candidate.o';base=inspect(basepath);basealloc=allocated(basepath)
        for label,entry in data['cells'].items():
            path=root/name/label/'candidate.o';q=inspect(path);a=allocated(path)
            assert q['records']==base['records'],(family,label,'symbol metadata')
            assert q['objects']==base['objects'],(family,label,'named object values/targets')
            raw_changed=[s for s in b.sizes(str(basepath)) if b.body(str(path),s)!=b.body(str(basepath),s)]
            changed=[s for s in raw_changed if b.verdict(*b.body(str(basepath),s),*b.body(str(path),s))[0]!='EXACT']
            allowed={'_ZNK3Psd14getFrequenciesEPff'} if family=='psd' else {'call_create','call_delete','call_run'}
            assert set(changed)<=allowed,(family,label,changed)
            assert set(a)==set(basealloc),(family,label,'allocated sections')
            delta={}
            for sec,v in a.items():
                if sec.startswith('.rodata.str'):
                    def strings(x):return Counter(bytes.fromhex(x['bytes']).split(b'\0'))-Counter({b'':bytes.fromhex(x['bytes']).split(b'\0').count(b'')})
                    before,after=strings(basealloc[sec]),strings(v)
                    assert not before-after,(family,label,'removed strings')
                    extra=after-before
                    assert set(extra)<=set(s.encode() for s in known),(family,label,extra)
                    delta[sec]=[s.decode() for s in extra.elements()]
                elif sec=='.rodata' and v!=basealloc[sec]:
                    assert 'call_run' in changed,(family,label,'unexplained table change')
                    assert v['bytes']==basealloc[sec]['bytes']
                    assert set(v['relocations'])==set(basealloc[sec]['relocations'])
                    for off,target in v['relocations'].items():
                        old=basealloc[sec]['relocations'][off]
                        assert target[:1]==old[:1] and target[1][:2]==old[1][:2],(family,label,'table owner',off)
                    delta[sec]='same table words/owners; changed call_run destination offsets recorded'
                else:assert v==basealloc[sec],(family,label,sec)
            q.update(allocated=a,changed_bodies=changed,raw_changed_bodies=raw_changed,expected_changes=delta)
            reports[family+'/'+label]=q;total+=1
            print(family,label,'functions',len(entry['verdicts']),'changed',changed,'named data',len(q['objects']))
assert total==34,total
winner=driver.ROOT/'build/gcc3-batch20-call-delete/call/restore-entry-debug/candidate.o'
assert b.verdict(*b.body(b.BLOB,'call_delete'),*b.body(str(winner),'call_delete'))==('EXACT',0)
(driver.ROOT/'build/batch20-root-full-tu-audit.json').write_text(json.dumps(reports,indent=2,default=lambda v: {'hex_bytes':v.hex()} if isinstance(v,bytes) else str(v))+'\n')
print('34/34 full TU metadata/data/allocated-section controls; call_delete exact158B positive fires')
