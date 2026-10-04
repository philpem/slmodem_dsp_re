#!/usr/bin/env python3
"""Trace two exact union-use recoveries through retained GCC3 RTL, no compile."""
import json,re
from pathlib import Path
import playbook_small_patterns as d
root=d.ROOT/'build/batch100-fax-idle-union';report={}
for n in ('17','27'):
 family='V'+n+'r_prc';fn='RxHdxIdleV'+n;offset=40 if n=='17' else 28
 record={}
 for stage in ('01.rtl','06.cse','09.loop'):
  row={}
  for cell in ('baseline','word-carrier-after-byte-store'):
   p=root/family/cell/(family+'.c.'+stage);s=p.read_text();parts=s.split(';; Function '+fn,1);assert len(parts)==2
   body=parts[1].split(';; Function ',1)[0]
   blocks=re.split(r'\n(?=\((?:insn|jump_insn|call_insn|note|code_label|barrier)\b)',body)
   field=[b for b in blocks if '(mem' in b and ('.result.word+'in b or '.result.byte.flags+'in b)]
   row[cell]={'jump_insns':len(re.findall(r'^\(jump_insn',body,re.M)),'status_memory_blocks':len(field),'word_memory_blocks':sum('.result.word+'in b for b in field),'byte_flag_memory_blocks':sum('.result.byte.flags+'in b for b in field)}
   if stage=='01.rtl':
    guard=[b for b in field if '.result.word+'in b]if cell!='baseline'else[field[-1]]
    assert len(guard)==1
    row[cell]['guard_rtl']=guard[0].strip()
    if cell=='baseline':assert '(mem/s:QI'in guard[0]
    else:assert '(mem/s:SI'in guard[0] and '(const_int '+str(offset)+' 'in guard[0]
  record[stage]=row
 assert record['06.cse']['baseline']['jump_insns']==record['06.cse']['word-carrier-after-byte-store']['jump_insns']==4
 assert record['09.loop']['baseline']['jump_insns']==5 and record['09.loop']['word-carrier-after-byte-store']['jump_insns']==4
 report[fn]=record
p=d.ROOT/'build/batch100-fax-union-rtl-trace.json';p.write_text(json.dumps(report,indent=2)+'\n')
print('RTL trace: 2 handlers x3 stages; initial byte-vs-word guard modes fire; after CSE 4/4 jumps, after GCSE+loop 5/4; later byte output verified EXACT in object matrix')
