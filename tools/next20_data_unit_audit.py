#!/usr/bin/env python3
"""Complete-TU invariants for bounded source refinements, including misses."""
import json
from collections import Counter
import playbook_small_patterns as d
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
s=(d.ROOT/'tools/batch20_data_gains_audit.py').read_text()
exec(s[s.index('b=d.b'):s.index('allowed_functions=')])
# Undefined typed objects have metadata but no section-backed bytes. Keep all
# records; inspect only concrete section owners as named allocated data.
inspect_source=(d.ROOT/'tools/playbook_bwch_unit_audit.py').read_text()
inspect_source=inspect_source[inspect_source.index('def inspect'):inspect_source.index('reports=')]
old="if v['st_info']['type']=='STT_OBJECT':"
assert inspect_source.count(old)==1
exec(inspect_source.replace(old,"if v['st_info']['type']=='STT_OBJECT' and isinstance(index,int):"))
import sys
reports={};count=0
for name in sys.argv[1:]:
 root=d.ROOT/'build'/name;p=json.loads((root/'results.json').read_text())
 for family,v in p['families'].items():
  base=canonical(root/family/'baseline/candidate.o')
  for label,c in v['cells'].items():
   x=canonical(root/family/label/'candidate.o')
   for k in ('records','objects','nontext','nontext_relocations','allocated_sizes'):
    expected=base[k]
    actual=x[k]
    if (name=='batch100-fax-frame-reverse' and 'literal' in label) or (name in ('batch100-fax-ring-writers','batch100-fax-fifo-entry','batch100-fax-write-frame') and label!='baseline') and k=='records':
     excluded={'faxvmi_byte_reverse'}
     if name=='batch100-fax-write-frame' and 'literal-fcs' in label:excluded.add('faxvmi_gen_fcs16')
     expected={n:v for n,v in expected.items() if n not in excluded}
    assert actual==expected,(name,family,label,k)
   added=Counter(map(repr,x['text_relocations']))-Counter(map(repr,base['text_relocations']))
   removed=Counter(map(repr,base['text_relocations']))-Counter(map(repr,x['text_relocations']))
   if (name=='batch100-fax-frame-reverse' and 'literal' in label) or (name in ('batch100-fax-ring-writers','batch100-fax-fifo-entry','batch100-fax-write-frame') and label!='baseline'):
    assert not added and all('faxvmi_byte_reverse' in r or (name=='batch100-fax-write-frame' and 'literal-fcs' in label and 'faxvmi_gen_fcs16' in r) for r in removed),(label,added,removed)
   elif name=='batch100-fax-tx-config-lifetime' and label=='config-after-transmitter':
    assert not added and removed==Counter({repr(('R_386_32',('symbol','FIFO_CFG',4))):1}),(name,label,added,removed)
   else:assert not added and not removed,(name,family,label,added,removed)
   assert not c.get('losses',[]),(name,family,label)
   reports[name+'/'+family+'/'+label]={'functions':len(c['functions']),'named_data':len(x['objects']),'changed':c.get('changed_bodies',[]),'gains':c.get('gains',[]),'removed_relocations':dict(removed)}
   count+=1
out=d.ROOT/'build/next20-data-unit-audit.json';out.write_text(json.dumps(reports,indent=2)+'\n')
print('Complete TU audit:',count,'cells; all metadata/named data/allocated nontext stable; all relocations accounted; no exact losses')
