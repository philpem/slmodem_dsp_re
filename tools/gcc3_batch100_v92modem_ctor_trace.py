#!/usr/bin/env python3
"""Assert V92 constructor child-input age and first sibling-call pass."""
import json,re
import playbook_small_patterns as d
out=d.ROOT/'build/gcc3-batch100-v92modem-ctor-terminal-owner';family='V92Modem';records=[]
labels=('baseline','terminal-return','descriptor-parameter','terminal-parameter')
for label in labels:
 terminal=label in ('terminal-return','terminal-parameter');input_owner=label in ('descriptor-parameter','terminal-parameter')
 for stage in ('01.rtl','02.sibling','35.mach'):
  text=(out/family/label/(family+'.cpp.'+stage)).read_text()
  chunks=[x for x in re.split(r'^;; Function ',text,flags=re.M)[1:] if x.startswith('V92Modem::V92Modem(')]
  assert len(chunks)==2,(label,stage,'ctor clones')
  for clone,body in enumerate(chunks):
   siblings=body.count('call_insn/j');assert siblings==(int(terminal) if stage!='01.rtl' else 0),(label,stage,clone,'sibling calls')
   reads=body.count('<variable>.dil+0')
   if stage=='01.rtl':
    assert reads==(1 if input_owner else 2),(label,clone,'descriptor member age')
    assert body.count('[ dilDescriptor ]')==(3 if input_owner else 2),(label,clone,'descriptor input carrier')
    assert body.count('call_placeholder')==13,(label,clone,'initial call alternatives')
   elif stage=='02.sibling':
    assert body.count('call_placeholder')==0,(label,clone,'sibling alternatives resolved')
    assert reads==(1 if input_owner else 2),(label,clone,'descriptor age retained')
   if terminal and stage!='01.rtl':
    calls=re.findall(r'\(call_insn/j[\s\S]*?(?=\n\n)',body);assert len(calls)==1 and '"dsplibs_debug_printf"' in calls[0],(label,stage,clone,'terminal diagnostic')
   records.append({'label':label,'stage':stage,'ctor_copy':clone,'sibling_calls':siblings,'descriptor_member_annotations':reads})
ledger=json.loads((out/'results.json').read_text());cells=ledger['families'][family]['cells']
for label in labels:
 ctor={n:v for n,v in cells[label]['verdicts'].items() if 'V92ModemC' in n};assert len(ctor)==2
 expected=('EXACT',0) if label=='terminal-parameter' else ('SIZE',{'baseline':40,'terminal-return':6,'descriptor-parameter':45}[label])
 assert all(tuple(v)==expected for v in ctor.values()),(label,ctor)
assert len(cells['terminal-parameter']['gains'])==2 and not cells['terminal-parameter']['losses']
(out/'ctor-source-stages.json').write_text(json.dumps({'records':records,'stage_controls':24,'initial_source_owner_controls':8,'crossed_object_controls':8,'first_sibling_pass':'02.sibling'},indent=2)+'\n')
print('24/24 stage controls; eight initial child-input ages and eight crossed ctor verdicts; terminal call selected at02.sibling')
