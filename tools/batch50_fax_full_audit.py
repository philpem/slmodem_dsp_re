#!/usr/bin/env python3
"""Full-TU audits for the eight bounded completed fax source domains."""
import json
from collections import Counter
import playbook_small_patterns as d
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
s=(d.ROOT/'tools/batch20_data_gains_audit.py').read_text()
exec(s[s.index('b=d.b'):s.index('allowed_functions=')])
roots=['boundaries','loops','controls','hdlc','fcs-fifo','null','idle','v29-flags']
reports={};count=0;gains={}
for root in roots:
 out=d.ROOT/('build/batch50-fax-'+root);r=json.loads((out/'results.json').read_text())
 for family,f in r['families'].items():
  base=canonical(out/family/'baseline/candidate.o')
  for label,c in f['cells'].items():
   q=canonical(out/family/label/'candidate.o');count+=1
   assert q['records']==base['records'],(root,family,label,'exports/types/bindings')
   assert q['objects']==base['objects'],(root,family,label,'named data')
   assert not c.get('losses',[]),(root,family,label,'loss')
   if root=='boundaries':allowed={family.replace('Vmi_v','v')+x+'_message' for x in ('rx','tx')} if family.startswith('Vmi_') else {family.replace('t_stc','TX_status')}
   elif root=='loops':allowed={{'V17t_int':'SMCv17_init','V29t_prc':'GenEQTrnSequenceV29','faxvmi_utls':'faxvmi_gen_fcs16'}[family]}
   elif root=='controls':allowed={family.replace('r_stc','RX_control')}
   elif root=='hdlc':allowed={'_hdlc_emulate_receive_state'}
   elif root=='fcs-fifo':allowed={'faxvmi_gen_fcs16' if family=='faxvmi_utls' else 'FIFO_write'}
   elif root=='null':allowed={'null_process' if family=='faxvmi_null' else '_rx_look_carrier_init'}
   elif root=='idle':allowed={'TxHdxIdle'+family.replace('t_prc','')}
   else:allowed={'RxHdxErrorV29','RxHdxIdleV29','RxHdxStartV29'} if label=='combined-winners' else {label}
   assert set(c.get('changed_bodies',[]))<=allowed,(root,family,label,'bystander change')
   changes={k:[base[k],q[k]] for k in q if q[k]!=base[k]}
   if root=='hdlc' and 'diagnostic' in label:
    for key in ('allocated_sizes','nontext','nontext_relocations'):
     a={k:v for k,v in base[key].items() if not k.startswith('.rodata.str')};z={k:v for k,v in q[key].items() if not k.startswith('.rodata.str')};assert a==z,(root,label,key)
    before=Counter(x for name,raw in base['nontext'].items() if name.startswith('.rodata.str') for x in bytes.fromhex(raw).split(b'\0') if x)
    after=Counter(x for name,raw in q['nontext'].items() if name.startswith('.rodata.str') for x in bytes.fromhex(raw).split(b'\0') if x)
    assert after-before==Counter({b'%2d.%02d[sec] Receive buffer OK in _hdlc_emulate_receive_state\n':1})
    added=Counter(map(repr,q['text_relocations']))-Counter(map(repr,base['text_relocations']))
    assert all(any(x in target for x in ['dsplibs_debug_level','dsplibs_debug_printf','Receive buffer OK']) for target in added),added
   else:
    assert Counter(map(repr,q['text_relocations']))==Counter(map(repr,base['text_relocations'])),(root,family,label,'relocation target multiset')
    for key in ['nontext','nontext_relocations','allocated_sizes']:
     if root=='v29-flags' and label=='RxNextStateV29' and key=='nontext_relocations':continue
     assert q[key]==base[key],(root,family,label,key)
   if c.get('gains'):
    assert all(k=='text_relocations' for k in changes),(root,family,label,'winning data change')
    for n in c['gains']:gains[n]=b.sizes(b.BLOB)[n]
   reports[root+'/'+family+'/'+label]={'functions':len(c['functions']),'named_data':len(q['objects']),'gains':c.get('gains',[]),'losses':c.get('losses',[]),'changed_bodies':c.get('changed_bodies',[]),'canonical_change_keys':list(changes)}
(d.ROOT/'build/batch50-fax-full-audit.json').write_text(json.dumps({'cells':count,'gains':gains,'reports':reports},indent=2)+'\n')
print(count,'valid cells; named data/export/ABI metadata equal; original HDLC diagnostic additions audited; negative RxNextState jump-table case offsets recorded; no exact losses;',len(gains),'unique gains;',sum(gains.values()),'blob bytes')
