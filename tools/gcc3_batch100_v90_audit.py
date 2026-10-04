#!/usr/bin/env python3
"""Complete TU audits for bounded batch100 V90 source controls."""
import json
import playbook_small_patterns as d
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
b=d.b
s=(d.ROOT/'tools/playbook_bwch_unit_audit.py').read_text();exec(s[s.index('def inspect'):s.index('reports=')])
s=(d.ROOT/'tools/gcc3_batch20_v90_small_audit.py').read_text();exec(s[s.index('def nontext'):s.index('\nreports=')])
reports={}
for suffix in ('v92modem-ctor-terminal','v92modem-ctor-terminal-owner','vpcm-tap-owner','vpcm-constellation-capture','cidcore-block-length','vpcm-phase2-owner-report','vpcm-probe-log-mode','demapper-byte-owner','adid-alt-init-dr','adid-reset-width','vpcm-probe-vector','vpcm-probe-final-store','vpcm-probe-level-mode','vpcm-probe-shared-result','fax-smc-capture-groups','fax-sgd-control-width','fax-sgd-det-count','fax-fse32-metric-lifetime','fax-fse-tick-abs','fax-fse16-owner','fax-sdm-feedback','fax-sdm-output-width','fax-fse32-distance','fax-sgd-symbol','fax-sgd-sequence','fax-smc-traversal-wrap','floatarma-count','v92jd-phase-result','v92jd-phase-scale','callprog-dualtone-cursor','callprog-cadence-interior','equalizer-entry','equalizer-length','generic-tone-scalar','generic-tone-mean','crc-shift','initiate-state','prefilter-type','std-verdict','std-fraction','std-scale','beta-lifetimes'):
 out=d.ROOT/'build'/('gcc3-batch100-'+suffix if (suffix.startswith(('callprog-','fax-','vpcm-','cid-','cidcore-','v92modem-')) or suffix in ('floatarma-count','v92jd-phase-result','v92jd-phase-scale')) else 'gcc3-batch100-v90-'+suffix);ledger=json.loads((out/'results.json').read_text())
 for family,f in ledger['families'].items():
  base=out/family/'baseline/candidate.o';meta=inspect(base);data=nontext(base)
  for label,c in f['cells'].items():
   obj=out/family/label/'candidate.o';m=inspect(obj);n=nontext(obj)
   assert m['records']==meta['records'] and m['objects']==meta['objects'],(suffix,family,label,'metadata/named objects')
   diffs={k:{'before':data.get(k),'after':n.get(k)} for k in set(data)|set(n) if data.get(k)!=n.get(k)}
   assert not diffs,(suffix,family,label,'nontext',list(diffs))
   assert m['allocated_sizes']==meta['allocated_sizes'],(suffix,family,label,'nontext sizes')
   before={name:b.verdict(*b.body(b.BLOB,name),*b.body(str(base),name)) for name in c['verdicts']};after={name:b.verdict(*b.body(b.BLOB,name),*b.body(str(obj),name)) for name in c['verdicts']}
   reports[suffix+'/'+family+'/'+label]={'functions':len(before),'exact_before':sum(v[0]=='EXACT' for v in before.values()),'exact_after':sum(v[0]=='EXACT' for v in after.values()),'gains':[name for name,v in after.items() if v[0]=='EXACT' and before[name][0]!='EXACT'],'losses':[name for name,v in before.items() if v[0]=='EXACT' and after[name][0]!='EXACT'],'changed':c.get('changed_bodies',[]),'nontext_changes':diffs}
(d.ROOT/'build/batch100-v90-complete-audit.json').write_text(json.dumps({'cells':len(reports),'reports':reports},indent=2)+'\n')
print(len(reports),'complete TU audits with unchanged metadata/named objects/nontext canonical relocations; every body scored')
