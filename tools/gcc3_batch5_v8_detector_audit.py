#!/usr/bin/env python3
"""Audit the bounded V8 detector full-TU controls and causal RTL checks."""
import json,re
import playbook_small_patterns as driver
from gcc3_reload_trace import instructions
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
b=driver.b
root=driver.ROOT/'build/gcc3-batch5-v8-detector'
ledger=json.loads((root/'results.json').read_text());cells=ledger['families']['V8Detector']['cells']
s=(driver.ROOT/'tools/playbook_bwch_unit_audit.py').read_text();exec(s[s.index('def inspect'):s.index('reports=')])
base=inspect(root/'V8Detector/baseline/candidate.o');reports={}
for label,c in cells.items():
 path=root/'V8Detector'/label/'candidate.o';report=inspect(path)
 assert report==base,(label,'metadata/data')
 with path.open('rb') as stream:
  elf=ELFFile(stream)
  nontext={x.name:x.data().hex() for x in elf.iter_sections() if x['sh_flags']&2 and x.name!='.text'}
  assert not any(isinstance(x,RelocationSection) and elf.get_section(x['sh_info']).name!='.text' for x in elf.iter_sections())
 if label=='baseline':baseline_nontext=nontext
 assert nontext==baseline_nontext,(label,'nontext')
 assert set(c.get('changed_bodies',[]))<= {'v8_phase_rev_init','v8_detectorinit','biquad_filter'}
 assert not c.get('losses')
 reports[label]={'metadata':report,'changed_bodies':c.get('changed_bodies',[]),'gains':c.get('gains',[])}
assert len(cells)==16
assert cells['combined-exact']['gains']==['v8_detectorinit','v8_phase_rev_init']

def stage(label,name,passname):
 text=(root/'V8Detector'/label/('V8Detector.c.'+passname)).read_text()
 chunks=[x for x in re.split(r'^;; Function ',text,flags=re.M)[1:] if x.splitlines()[0].strip()==name]
 assert len(chunks)==1,(label,name,passname)
 return instructions(';; Function '+chunks[0])

def nodes(x):
 if isinstance(x,list):
  yield x
  for y in x[1:]:yield from nodes(y)

controls={}
# The field bound is loop-invariant/constant-propagated but remains a register
# compare in combine. A literal bound is compared immediately with 63.
for label in ('baseline','phase-field-bound','phase-cached-bound'):
 p=stage(label,'v8_phase_rev_init','20.combine')
 comparisons=[v for v in p.values() if v[0]=='set' and any(x[0].startswith('compare:') for x in nodes(v))]
 controls[label+'/phase-comparisons']=comparisons
 assert len(comparisons)==1
 if label=='phase-field-bound':assert not any(x[0]=='const_int' for x in nodes(comparisons[0]))
 else:assert any(x[0]=='const_int' and x[1]==('63' if label=='baseline' else '64') for x in nodes(comparisons[0]))
for label in ('baseline','detector-int-empty-1-order-0','detector-short-empty-0-order-0','detector-short-empty-1-order-0'):
 p=stage(label,'v8_detectorinit','20.combine')
 extensions=[v for v in p.values() if v[0]=='set' and any(x[0]=='sign_extend:SI' for x in nodes(v)) and any(n in v[1] for n in ('i','j'))]
 controls[label+'/counter-extensions']=len(extensions)
 expected={'baseline':0,'detector-int-empty-1-order-0':0,'detector-short-empty-0-order-0':3,'detector-short-empty-1-order-0':4}[label]
 assert len(extensions)==expected,(label,len(extensions),expected)
biquad_root=driver.ROOT/'build/gcc3-batch5-v8-biquad'
biquad_cells=json.loads((biquad_root/'results.json').read_text())['families']['V8Detector']['cells']
assert len(biquad_cells)==4 and len({v['source_hash'] for v in biquad_cells.values()})==4
assert len({v['object_hash'] for v in biquad_cells.values()})==1
for label,c in biquad_cells.items():
 path=biquad_root/'V8Detector'/label/'candidate.o'
 assert path.read_bytes()==(root/'V8Detector/baseline/candidate.o').read_bytes()
 assert inspect(path)==base and not c.get('changed_bodies',[])
controls['biquad-call-result-raw-identity']=4
summary={'valid_cells':len(cells)+len(biquad_cells),'source_hashes':len({x['source_hash'] for x in cells.values()}),'object_hashes':len({x['object_hash'] for x in cells.values()}),'functions_per_cell':len(cells['baseline']['functions']),'named_data_per_cell':len(base['objects']),'causal_checks':len(controls),'controls':controls,'reports':reports}
(root/'full-tu-audit.json').write_text(json.dumps(summary,indent=2)+'\n')
print(f"{len(cells)+len(biquad_cells)}/{len(cells)+len(biquad_cells)} full TUs: {summary['functions_per_cell']} functions/{summary['named_data_per_cell']} named data; metadata, bytes, relocation targets, bystanders preserved")
print(f"{len(controls)}/{len(controls)} causal RTL controls passed; {summary['source_hashes']} source hashes/{summary['object_hashes']} objects; combined gains {cells['combined-exact']['gains']}")
