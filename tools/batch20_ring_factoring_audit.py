#!/usr/bin/env python3
"""Complete object proof for the bounded Ring Reset duplication control."""
import json
import playbook_small_patterns as d
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
s=(d.ROOT/'tools/batch20_data_gains_audit.py').read_text();exec(s[s.index('b=d.b'):s.index('allowed_functions=')])
root=d.ROOT/'build/batch20-ring-factoring';l=json.loads((root/'results.json').read_text())['families']['ringDetector']['cells'];base=canonical(root/'ringDetector/baseline/candidate.o');q=canonical(root/'ringDetector/body/candidate.o')
for k in ['objects','records','nontext','nontext_relocations','allocated_sizes']:assert q[k]==base[k],k
from collections import Counter
add=Counter(map(str,q['text_relocations']))-Counter(map(str,base['text_relocations']));remove=Counter(map(str,base['text_relocations']))-Counter(map(str,q['text_relocations']))
assert sum(remove.values())==1 and all('RingDetector_Reset' in x for x in remove),remove
assert sum(add.values())==3 and all(any(y in x for y in ['dsplibs_debug_level','dsplibs_debug_printf','Reset Soft Ring']) for x in add),add
assert set(l['body']['changed_bodies'])=={'RingDetector_Create'}
assert not l['body']['losses'];assert l['body']['verdicts']['RingDetector_Delete']==['EXACT',0];assert l['body']['verdicts']['RingDetector_GetLastRing']==['EXACT',0]
(root/'full-audit.json').write_text(json.dumps({'baseline':base,'body':q,'cells':l},indent=2,default=str)+'\n')
for label,c in l.items():print(label,'5functions;',c['verdicts'])
print('Full object audit: metadata/data/nontext/canonicalnontextrelocations identical; text removesResetcall/adds exactly original3diagnosticrelocs; onlyCreate body changes; twoexact positive controls retained.')
