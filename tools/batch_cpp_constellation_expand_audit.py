#!/usr/bin/env python3
"""Audit fixed mixed-radix expansion and its changed inlining/jump-table caller."""
import json
from pathlib import Path
import playbook_small_patterns as d
from gcc3_value_carriers_audit import inspect,function_chunk
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
import jumptable
TARGET='_ZN21V90ConstellationPower21calcModulusParametersEP16V90MappingParams'
CALLER='_ZN21V90ConstellationPower8getPowerEP16V90MappingParams26V90TxPowerMeasurementPoint7PcmType'
def cases(path):
 with path.open('rb') as stream:
  elf=ELFFile(stream);tab=elf.get_section_by_name('.symtab');sym=tab.get_symbol_by_name(CALLER)[0];text=elf.get_section(sym['st_shndx']);start=sym['st_value'];end=start+sym['st_size'];sites=[]
  for section in elf.iter_sections():
   if not isinstance(section,RelocationSection) or section['sh_info']!=sym['st_shndx']:continue
   for rel in section.iter_relocations():
    offset=rel['r_offset'];target=tab.get_symbol(rel['r_info_sym'])
    if start<=offset<end and isinstance(target['st_shndx'],int) and elf.get_section(target['st_shndx']).name=='.rodata':
     assert rel['r_info_type']==1
     sites.append(int.from_bytes(text.data()[offset:offset+4],'little')+target['st_value'])
  assert len(sites)==1,(path,sites)
  ro=elf.get_section_by_name('.rodata');base=sites[0];owner=elf.get_section_by_name('.rel.rodata');lookup={r['r_offset']:r for r in owner.iter_relocations()};addresses=[]
  starts={row[0] for row in jumptable.instructions(str(path),text.name,start,end)}
  for i in range(6):
   offset=base+4*i;rel=lookup[offset];dest=tab.get_symbol(rel['r_info_sym']);assert rel['r_info_type']==1 and dest['st_shndx']==sym['st_shndx']
   absolute=int.from_bytes(ro.data()[offset:offset+4],'little')+dest['st_value'];assert absolute in starts and start<=absolute<end,(path,i,absolute)
   addresses.append(absolute-start)
  return addresses

def main():
 root=d.ROOT/'build/batch-cpp-constellation-expand';folder=root/'V90ConstellationPower';x=json.loads((root/'results.json').read_text())['families']['V90ConstellationPower'];base=folder/'baseline/candidate.o';assert base.read_bytes()==(folder/'retained.o').read_bytes();bi=inspect(base);refcases=cases(Path(d.b.BLOB));report={};total=0
 for label,cell in x['cells'].items():
  obj=folder/label/'candidate.o';ci=inspect(obj);fixed={key:ci[key]==bi[key] for key in ['records','allocated','nobits']};assert all(fixed.values()),(label,fixed)
  deltas=[];assert len(bi['relocations'])==len(ci['relocations'])
  for before,after in zip(bi['relocations'],ci['relocations']):
   if before!=after:
    assert before[:3]==after[:3] and before[0]=='.rodata' and before[3][:2]==after[3][:2]==('audited-code-destination',CALLER),(label,before,after)
    deltas.append([before,after])
  cs=cases(obj)
  if label!='baseline':assert cs==refcases and len(deltas)==6,(label,cs,refcases)
  names=d.b.sizes(str(base));total+=len(names)
  changed=sorted(n for n in names if d.b.body(str(base),n)!=d.b.body(str(obj),n));assert changed==sorted(cell.get('changed_bodies',[]));assert set(changed)<=set([TARGET,CALLER])
  rtl=function_chunk(folder/label/'V90ConstellationPower.cpp.01.rtl','calcModulusParameters');fields=[name for line in rtl.splitlines() for name in ['shaperSR','word_0'] if '.'+name+'+0' in line]
  report[label]={'emitted_bodies':len(names),'fixed_invariants':fixed,'changed_bodies':changed,'case_offsets':cs,'original_case_offsets':refcases,'changed_table_relocations':deltas,'initial_sum_field_order':fields,'target_grade':cell['verdicts'][TARGET],'caller_grade':cell['verdicts'][CALLER],'gains':cell.get('gains',[]),'losses':cell.get('losses',[])}
 assert report['expanded-sum-original-loads']['target_grade']==['EXACT',0] and not report['expanded-sum-original-loads']['losses']
 assert report['digits1-place1']['target_grade']==['BYTES',7]
 report['emitted_body_denominator']=total
 (root/'audit.json').write_text(json.dumps(report,indent=2)+'\n')
 print('Constellation audit:5cells,%demitted bodies,rawbaseline reproduced; symbols/data/BSS fixed. All6changed caller table relocations target exact original case offsets, boundaries checked;7other bodies unchanged. One complete622B gain,0losses.'%total)
if __name__=='__main__':main()
