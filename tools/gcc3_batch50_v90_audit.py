#!/usr/bin/env python3
"""Audit finite V90 source domains and reviewed anonymous table movement."""
import json
from pathlib import Path
import playbook_small_patterns as d
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
b=d.b
s=(d.ROOT/'tools/playbook_bwch_unit_audit.py').read_text();exec(s[s.index('def inspect'):s.index('reports=')])
s=(d.ROOT/'tools/gcc3_batch20_v90_small_audit.py').read_text();exec(s[s.index('def nontext'):s.index('\nreports=')])
reports={};cells=0
for suffix in ('masks','prefilter','cycle','vector','vector-direct','mask-index','cp','cp-owner','cp-owner-cross','cp-index','dispatch','k-history','k-narrow','session','prefilter-cfg','adid-verdict','adid-cross','echo-frac','sd-owner','sd-boundaries','spectral-coeff','sd-count','sd-accept','spectral-native','rdetector','ja-narrow','printbase','jd-phase','jd-shift','rate-half'):
 out=d.ROOT/'build'/('gcc3-batch50-v90-'+suffix)
 if not (out/'results.json').exists():continue
 ledger=json.loads((out/'results.json').read_text())
 for family,f in ledger['families'].items():
  base=out/family/'baseline/candidate.o';meta=inspect(base);data=nontext(base)
  for label,c in f['cells'].items():
   obj=out/family/label/'candidate.o';m=inspect(obj);n=nontext(obj)
   assert m['records']==meta['records'] and m['objects']==meta['objects'],(suffix,label,'metadata/named objects')
   diffs={k:{'before':data.get(k),'after':n.get(k)} for k in set(data)|set(n) if data.get(k)!=n.get(k)}
   if suffix=='echo-frac':
    assert set(diffs)<={'.rodata.cst4','.rodata.cst8','.rodata.cst16'},(suffix,label)
   else:
    assert m['allocated_sizes']==meta['allocated_sizes'],(suffix,label,'nontext sizes')
    assert all(k=='.rodata' or k.startswith('.gnu.linkonce.t.') or k=='.rodata.str1.4' and suffix.startswith('adid-') for k in diffs),(suffix,label)
    for sec,q in diffs.items():
     if sec=='.rodata.str1.4':
      for string_obj in (base,obj):
       with string_obj.open('rb') as stream:
        string_section=ELFFile(stream).get_section_by_name(sec)
        assert string_section['sh_flags'] & 0x30 == 0x30, 'merge/string canonicalization requires both ELF flags'
      assert q['before']['relocations']==q['after']['relocations']=={}
      old_strings=bytes.fromhex(q['before']['bytes']);new_strings=bytes.fromhex(q['after']['bytes'])
      assert sorted(x for x in old_strings.split(b'\0') if x)==sorted(x for x in new_strings.split(b'\0') if x)
      continue
     if sec.startswith('.gnu.linkonce.t.'):
      assert sec[len('.gnu.linkonce.t.'):] in c.get('changed_bodies',[])
      assert q['before']['relocations']==q['after']['relocations']
      continue
     assert q['before']['bytes']==q['after']['bytes']
     assert set(q['before']['relocations'])==set(q['after']['relocations'])
     assert all(x[0]=='R_386_32' and x[1][0]=='function' for v in q.values() for x in v['relocations'].values())
     assert all(q['before']['relocations'][off][1][1]==q['after']['relocations'][off][1][1] for off in q['before']['relocations'])
   before={name:b.verdict(*b.body(b.BLOB,name),*b.body(str(base),name)) for name in c['verdicts']}
   after={name:b.verdict(*b.body(b.BLOB,name),*b.body(str(obj),name)) for name in c['verdicts']}
   gains=[name for name,v in after.items() if v[0]=='EXACT' and before[name][0]!='EXACT']
   losses=[name for name,v in before.items() if v[0]=='EXACT' and after[name][0]!='EXACT']
   reports[suffix+'/'+family+'/'+label]={'functions':len(b.sizes(str(obj))),'gains':gains,'losses':losses,'changed':c.get('changed_bodies',[]),'nontext_changes':diffs}
   cells+=1
(d.ROOT/'build/batch50-v90-complete-audit.json').write_text(json.dumps({'cells':cells,'reports':reports},indent=2)+'\n')
print(str(cells)+' completeTU audits: symbol metadata/named data unchanged; reviewed table destination shifts and negative precision constant-pool changes retained')
