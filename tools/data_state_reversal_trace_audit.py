#!/usr/bin/env python3
"""Full V32 state objects and balanced-RTL field/publication observations."""
import json,re
from pathlib import Path
import playbook_small_patterns as d
import gcc3_reload_trace as r
from gcc3_value_carriers_audit import inspect
from elftools.elf.elffile import ELFFile

def data_shapes(path):
 with path.open("rb") as stream:
  elf=ELFFile(stream)
  return {s.name:[s["sh_type"],s["sh_flags"],s["sh_addralign"],s["sh_size"]] for s in elf.iter_sections() if s["sh_flags"] & 2 and not s["sh_flags"] & 4}

FIELDS=('short_ac','short_a8','symbol_len','rtd')
STAGES=('01.rtl','06.cse','08.gcse','14.ce1','20.combine','25.greg','27.flow2','33.sched2','35.mach')

def memory(node):return isinstance(node,list) and node and node[0].startswith('mem')
def observations(pattern):
 records=[]
 def visit(node,parent=None,index=None):
  if not isinstance(node,list) or not node:return
  if memory(node):
   fields=[name for name in FIELDS if any(name in str(x) for x in node[2:])]
   for field in fields:
    records.append({'field':field,'role':'store' if parent and parent[0]=='set' and index==1 else 'read','mode':node[0],'expression':node})
  for i,c in enumerate(node):visit(c,node,i)
 visit(pattern)
 return records

def main():
 result={'cells':0,'bodies':0,'raw_baselines':0,'trace_function_stages':0,'unparsed_stages':[],'gains':[],'losses':[],'allocated_string_permutations':[],'families':{}}
 for package in ('data-state-reversal','data-state-reversal-width'):
  root=d.ROOT/'build'/package;ledger=json.loads((root/'results.json').read_text());family=ledger['families']['V32rxhdx'];base=root/'V32rxhdx/baseline/candidate.o';meta=inspect(base);shapes=data_shapes(base)
  assert base.read_bytes()==(root/'V32rxhdx/retained.o').read_bytes();result['raw_baselines']+=1
  rows={}
  for label,cell in family['cells'].items():
   cd=root/'V32rxhdx'/label;obj=cd/'candidate.o';view=inspect(obj)
   assert data_shapes(obj)==shapes,(package,label,'allocated section shapes')
   for key in ('records','nobits','relocations'):assert view[key]==meta[key],(package,label,key)
   assert set(view['allocated'])==set(meta['allocated'])
   for section, before in meta['allocated'].items():
    after=view['allocated'][section]
    if before==after:continue
    # Preserve raw differences. Only these two NUL-terminated diagnostic
    # strings may permute; no bytes, lengths or section membership may change.
    assert section=='.rodata.str1.1',(package,label,section)
    old=bytes.fromhex(before);new=bytes.fromhex(after)
    assert len(old)==len(new) and sorted(old.split(b'\0'))==sorted(new.split(b'\0'))
    result['allocated_string_permutations'].append({'package':package,'cell':label,'section':section,'before':before,'after':after})
   changed=sorted(n for n in cell['functions'] if d.b.body(str(obj),n)!=d.b.body(str(base),n))
   assert changed==sorted(cell.get('changed_bodies',[])) and set(changed)<= {'RxHdxPhsReversal'}
   assert not cell.get('gains',[]) and not cell.get('losses',[])
   result['cells']+=1;result['bodies']+=len(cell['functions'])
   traces={}
   for stage in STAGES:
    text=r.function((cd/('V32rxhdx.c.'+stage)).read_text(),'RxHdxPhsReversal')
    # GCSE prose prints removed insns before the actual stream; require one
    # explicit function-begin marker instead of selecting a duplicate UID.
    markers=list(re.finditer(r'(?m)^\(note[^\n]+NOTE_INSN_FUNCTION_BEG\)[ \t]*$',text))
    if len(markers)!=1:
     result['unparsed_stages'].append([package,label,stage,len(markers),'multiple complete compiler streams'])
     traces[stage]={'status':'unparsed','function_begin_markers':len(markers),'reason':'multiple complete compiler streams'}
     continue
    nodes=r.instructions(text[markers[0].end():])
    records=[]
    for uid,pattern in nodes.items():
     for row in observations(pattern):records.append({'uid':uid,**row})
    assert records,(package,label,stage,'silent field trace')
    traces[stage]=records;result['trace_function_stages']+=1
   rows[label]={'verdict':cell['verdicts']['RxHdxPhsReversal'],'changed':changed,'traces':traces}
  result['families'][package]=rows
 assert (d.ROOT/'build/data-state-reversal/V32rxhdx/persistent-1-publication-1/candidate.o').read_bytes()==(d.ROOT/'build/data-state-reversal-width/V32rxhdx/graph-1-shared-0-report-0/candidate.o').read_bytes()
 assert result['cells']==12 and result['bodies']==168 and result['trace_function_stages']==96 and len(result['unparsed_stages'])==12
 # Known source field-publication difference must be observed in initial RTL.
 baseline=result['families']['data-state-reversal']['baseline']['traces']['01.rtl']
 persistent=result['families']['data-state-reversal']['persistent-1-publication-0']['traces']['01.rtl']
 assert sum(x['field']=='short_ac' and x['role']=='store' for x in baseline)==2
 assert sum(x['field']=='short_ac' and x['role']=='store' for x in persistent)==3,'known persistent field control did not fire'
 result['repeated_graph_control_raw_equal']=True;result['known_publication_control_fired']=True
 (d.ROOT/'build/data-state-reversal-trace-audit.json').write_text(json.dumps(result,indent=2)+'\n')
 print('V32 complete audit:',result['cells'],'TUs/',result['bodies'],'bodies;',result['raw_baselines'],'raw baselines;',result['trace_function_stages'],'field-stage observations; zero gains/losses')
 for package,fam in result['families'].items():
  for label,c in fam.items():
   print(package,label,c['verdict'],'short_ac store counts',[(s,sum(x['field']=='short_ac' and x['role']=='store' for x in rows)) for s,rows in c['traces'].items() if isinstance(rows,list)])
if __name__=='__main__':main()
