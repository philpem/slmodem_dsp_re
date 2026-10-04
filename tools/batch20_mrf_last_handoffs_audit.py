#!/usr/bin/env python3
"""Full-TU proof for final two MRF handoff boundary controls."""
import json
from collections import Counter
import playbook_small_patterns as d
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
s=(d.ROOT/'tools/batch20_data_gains_audit.py').read_text();exec(s[s.index('b=d.b'):s.index('allowed_functions=')])
root=d.ROOT/'build/batch20-mrf-last-handoffs';ledger=json.loads((root/'results.json').read_text());reports={}
for family,entry in ledger['families'].items():
 base=canonical(root/family/'baseline/candidate.o')
 for label,c in entry['cells'].items():
  q=canonical(root/family/label/'candidate.o');combined=family=='v23rx' and label.startswith('combined');ex={'dsplibs_debug_level','dsplibs_debug_printf'} if combined else set()
  assert {k:v for k,v in q['records'].items() if k not in ex}=={k:v for k,v in base['records'].items() if k not in ex},(family,label,'records')
  assert q['objects']==base['objects'],(family,label,'objects')
  for k in ['allocated_sizes','nontext','nontext_relocations']:assert {s:v for s,v in q[k].items() if not(combined and s.startswith('.rodata.str'))}=={s:v for s,v in base[k].items() if not(combined and s.startswith('.rodata.str'))},(family,label,k)
  add=Counter(map(str,q['text_relocations']))-Counter(map(str,base['text_relocations']));remove=Counter(map(str,base['text_relocations']))-Counter(map(str,q['text_relocations']))
  assert not remove,(family,label,remove)
  assert sum(add.values())==(3 if combined else 0),(family,label,add)
  assert all(any(t in x for t in ['dsplibs_debug_level','dsplibs_debug_printf','V23FP Rx Created']) for x in add),(family,label,add)
  assert set(c.get('changed_bodies',[]))<=({'cid_modem'} if family=='Rxcid' else {'v23FP_rx_create','v23FP_rx_progress'}),(family,label,c.get('changed_bodies'))
  assert not c.get('losses',[]),(family,label,c.get('losses'))
  assert c.get('gains',[])==(['v23FP_rx_create'] if combined else []),(family,label,c.get('gains'))
  reports[family+'/'+label]={'functions':len(c['functions']),'verdicts':c['verdicts'],'changed_bodies':c.get('changed_bodies',[]),'gains':c.get('gains',[]),'losses':c.get('losses',[]),'metadata_data_relocs_stable':True}
assert len(reports)==6
(root/'full-audit.json').write_text(json.dumps(reports,indent=2)+'\n')
print('MRF last-handoff full-TU audit:6cells; onlymeasuredcaller/knownconstructorchanges; originalctorgainpreserved; zeroadditionalgains/losses.')
