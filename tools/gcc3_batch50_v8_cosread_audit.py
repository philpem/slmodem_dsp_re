#!/usr/bin/env python3
"""Audit widened cosine formals and complete caller TUs, including switch tables."""
import json
from collections import Counter
import playbook_small_patterns as d
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
b=d.b;s=(d.ROOT/'tools/playbook_bwch_unit_audit.py').read_text();exec(s[s.index('def inspect'):s.index('reports=')])
reports={};total=0
for run,expected in [('v8-cosread',18),('v8-tone',9),('v8-ansam-phase',5),('integer-production',12)]:
 root=d.ROOT/('build/gcc3-batch50-'+run);ledger=json.loads((root/'results.json').read_text());count=0
 for family,fr in ledger['families'].items():
  base=None
  for label,c in fr['cells'].items():
   p=root/family/label/'candidate.o';q=inspect(p);q['nontext']={};q['relocs']={}
   with p.open('rb') as stream:
    elf=ELFFile(stream);tab=elf.get_section_by_name('.symtab');names={i:s.name for i,s in enumerate(elf.iter_sections())};symbols=list(tab.iter_symbols());text_index=elf.get_section_index('.text')
    for idx,x in enumerate(elf.iter_sections()):
     if not x['sh_flags']&2 or x.name=='.text':continue
     raw=bytearray(x.data());targets={}
     for rs in elf.iter_sections():
      if not isinstance(rs,RelocationSection) or rs['sh_info']!=idx:continue
      for rel in rs.iter_relocations():
       off=rel['r_offset'];sym=tab.get_symbol(rel['r_info_sym']);kind={1:'R_386_32',2:'R_386_PC32'}[rel['r_info_type']];target=b.relocation_target(kind,sym.name or names[sym['st_shndx']],bytes(raw[off:off+4]),b.section_symbols(str(p)))
       if target[:2]==('section','.text'):
        addr=target[2];owners=[s for s in symbols if s['st_info']['type']=='STT_FUNC' and s['st_shndx']==text_index and s['st_value']<=addr<s['st_value']+s['st_size']];assert len(owners)==1
        target=('function',owners[0].name,addr-owners[0]['st_value'])
       targets[off]=(kind,target);raw[off:off+4]=b'\0'*4
     q['nontext'][x.name]=sorted((part.hex(),n) for part,n in Counter(bytes(v) for v in raw.split(b'\0')).items()) if x['sh_flags']&32 else raw.hex()
     q['relocs'][x.name]=targets
   if label=='baseline':base=q
   for k in ['records','objects','allocated_sizes','nontext']:assert q[k]==base[k],(run,family,label,k)
   changes={k:{off:[base['relocs'][k].get(off),v] for off,v in values.items() if v!=base['relocs'][k].get(off)} for k,values in q['relocs'].items() if values!=base['relocs'][k]}
   for section,entries in changes.items():
    assert set(q['relocs'][section])==set(base['relocs'][section])
    for before,after in entries.values():assert before[0]==after[0] and before[1][:2]==after[1][:2]==('function','v8handshak'),(run,family,label,'new table owner')
   allowed={'v8_TONEq_generate','v8_ansamgenerate','v8handshak'} if family=='V8' else {'v8_dftupdate'} if family=='V8Dftc' else {'charFlip'} if family=='V8global' else {'notch_filter'} if family=='V8Detector' else {'V34EchoEstimateDelayLineEnergy','V34EchoReportCoeff'} if family=='v34filters' else set()
   assert set(c.get('changed_bodies',[]))<=allowed and not c.get('losses')
   if family=='V8Dftc':assert c['verdicts']['v8_cosread']==['EXACT',0]
   reports[run+'/'+family+'/'+label]={'changed_bodies':c.get('changed_bodies',[]),'gains':c.get('gains',[]),'table_target_changes':changes,'functions':len(c['functions']),'data':len(q['objects'])};total+=1;count+=1
  assert fr['cells']['baseline']['baseline_reproduced']
 assert count==expected
assert total==44
(d.ROOT/'build/batch50-v8-cosread-complete-audit.json').write_text(json.dumps({'valid_tus':total,'reports':reports},indent=2)+'\n')
print('44/44 full-caller/TU audits pass, raw baselines, metadata/data/nontext/relocs preserved, switch owner/extents checked, callee14B exact, zero exact losses')
