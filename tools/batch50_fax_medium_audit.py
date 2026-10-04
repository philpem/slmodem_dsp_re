#!/usr/bin/env python3
"""Full-TU audits of three fresh medium completed-fax domains."""
import json
from collections import Counter
import playbook_small_patterns as d
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
s=(d.ROOT/'tools/batch20_data_gains_audit.py').read_text()
exec(s[s.index('b=d.b'):s.index('allowed_functions=')])
roots=['tx-templates','tx-mode-dispatch','hdlc-frame-guard','medium-reads'];count=0;reports={}
for root in roots:
 out=d.ROOT/('build/batch50-fax-'+root);r=json.loads((out/'results.json').read_text())
 for family,f in r['families'].items():
  base=canonical(out/family/'baseline/candidate.o')
  allowed={'V17TX_create'} if root in ('tx-templates','tx-mode-dispatch') else {'_hdlc_receive_state'} if root=='hdlc-frame-guard' else {'_tx_scrambled_ones_state'} if family=='cDATAtx' else {'faxvmi_hdlc_frame'}
  for label,c in f['cells'].items():
   q=canonical(out/family/label/'candidate.o');count+=1
   for k in ('objects','nontext','nontext_relocations','allocated_sizes'):
    if k=='nontext':
     assert {n:v for n,v in q[k].items() if not n.startswith('.rodata.str')}=={n:v for n,v in base[k].items() if not n.startswith('.rodata.str')}
     bag=lambda obj:Counter(x for n,v in obj[k].items() if n.startswith('.rodata.str') for x in bytes.fromhex(v).split(b'\0') if x)
     assert bag(q)==bag(base),(root,family,label,'string values/counts')
    else:assert q[k]==base[k],(root,family,label,k)
   if root=='tx-templates':
    assert {k:v for k,v in q['records'].items() if k!='SDM_CFG'}=={k:v for k,v in base['records'].items() if k!='SDM_CFG'}
    if 'SDM_CFG' in q['records']:assert q['records']['SDM_CFG']==['STT_NOTYPE','STB_GLOBAL','STV_DEFAULT','SHN_UNDEF',0]
   else:assert q['records']==base['records'],(root,family,label,'records')
   # Whole-template source controls restore original FIFO_CFG+4/SDM_CFG references;
   # their named owner/offset additions are the measured source mechanism.
   before=Counter(map(repr,base['text_relocations']));after=Counter(map(repr,q['text_relocations']))
   if root=='tx-templates':
    additions=after-before;removals=before-after
    assert all(any(x in k for x in ('FIFO_CFG','SDM_CFG','SMCv17_CFG')) for k in additions|removals),(label,additions,removals)
   else:assert after==before,(root,family,label,'relocation multiset')
   assert not c.get('gains') and not c.get('losses'),(root,family,label,'exact set changed')
   assert set(c.get('changed_bodies',[]))<=allowed,(root,family,label,'bystanders')
   reports[root+'/'+family+'/'+label]={'functions':len(c['functions']),'changed_bodies':c.get('changed_bodies',[]),'verdicts':{n:c['verdicts'][n] for n in allowed},'string_pool_order_changed':q['nontext']!=base['nontext'],'added_text_relocations':dict(after-before),'removed_text_relocations':dict(before-after)}
(d.ROOT/'build/batch50-fax-medium-audit.json').write_text(json.dumps({'cells':count,'reports':reports,'gains':[],'losses':[]},indent=2)+'\n')
print(count,'valid complete-TU cells; named metadata/data and nonstring sections equal; string values/counts and original template reference additions audited; no bystander changes/exact gains/losses')
