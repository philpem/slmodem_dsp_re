#!/usr/bin/env python3
"""Audit all finite V8 interface source controls without source adoption."""
import json
from collections import Counter
import playbook_small_patterns as d
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
s=(d.ROOT/'tools/batch20_data_gains_audit.py').read_text();exec(s[s.index('b=d.b'):s.index('allowed_functions=')])
reports={}
for name,allowed in [('batch20-v8-interface',{'V8GetMessage'}),('batch20-v8-getword',{'V8GetMessage'}),('batch20-v8-control',{'V8Control'}),('batch20-v8-set',{'V8SetMessage','V8Control'}),('batch20-v8-getlength',{'V8GetMessage'})]:
 root=d.ROOT/'build'/name;l=json.loads((root/'results.json').read_text())['families']['V8Interface']['cells'];base=canonical(root/'V8Interface/baseline/candidate.o')
 for label,c in l.items():
  q=canonical(root/'V8Interface'/label/'candidate.o')
  for k in ['records','objects','allocated_sizes','nontext_relocations']:assert q[k]==base[k],(name,label,k)
  for sec,raw in q['nontext'].items():
   if sec.startswith('.rodata.str'):
    assert Counter(bytes.fromhex(raw).split(b'\0'))==Counter(bytes.fromhex(base['nontext'][sec]).split(b'\0')),(name,label,sec)
   else:assert raw==base['nontext'][sec],(name,label,sec)
  addition=Counter(map(str,q['text_relocations']))-Counter(map(str,base['text_relocations']));removal=Counter(map(str,base['text_relocations']))-Counter(map(str,q['text_relocations']))
  assert not removal and all('dsplibs_debug_level' in t for t in addition),(name,label,addition,removal)
  assert sum(addition.values())<=1,(name,label,addition)
  assert set(c.get('changed_bodies',[]))<=allowed,(name,label,c.get('changed_bodies'))
  assert not c.get('gains',[]) and not c.get('losses',[]),(name,label)
  reports[name+'/'+label]={'functions':len(c['functions']),'verdicts':c['verdicts'],'changed_bodies':c.get('changed_bodies',[]),'metadata_data_relocs_stable':True}
assert len(reports)==24
(d.ROOT/'build/batch20-v8-interface-audit.json').write_text(json.dumps(reports,indent=2)+'\n')
print('V8 interface full-TU audit:24cells×7functions; metadata/nameddata/nonstrings/canonical import/string targets retained; existingstring permutation and atmost1duplicated debuggate only; targetbodyplusdeclaredControlscratchbystanderchanges; 2baselineexact retained; nogains/losses.')
