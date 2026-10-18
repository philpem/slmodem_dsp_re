#!/usr/bin/env python3
"""Audit the four SGD source controls; retain every raw mismatch explicitly."""
import json
import playbook_small_patterns as d
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
from gcc3_alignment_scratch_model import transfer
from gcc3_mechanism_scratch_observe import validate_trace
# Reuse the independently negative-controlled complete-object metadata inspector.
s=(d.ROOT/'tools/sgd_structure293_audit.py').read_text()
exec(s[s.index('def inspect'):s.index('\nout={}')])
root=d.ROOT/'build/sgd-sequence293';cells=json.loads((root/'results.json').read_text())['families']['Sgd']['cells']
base=inspect(root/'Sgd/baseline/candidate.o');exact={n for n,v in cells['baseline']['verdicts'].items() if v[0]=='EXACT'}
assert len(cells)==4 and len(exact)==3
for name,entry in cells.items():
 assert len(entry['verdicts'])==9 and {n for n,v in entry['verdicts'].items() if v[0]=='EXACT'}==exact
 assert set(entry.get('changed_bodies',[])) <= {'SGD_sequence_gen','SGD_correlate'}
 assert inspect(root/'Sgd'/name/'candidate.o')==base
candidate=str(root/'Sgd/postdecrement-expression/candidate.o')
a,ar=d.b.body(d.b.BLOB,'SGD_sequence_gen');b,br=d.b.body(candidate,'SGD_sequence_gen')
assert len(a)==len(b)==246 and ar==br=={}
diffs=[(i,x,y) for i,(x,y) in enumerate(zip(a,b)) if x!=y]
suffix=[item for item in diffs if item[0]>=0x49]
assert len(diffs)==47
assert suffix==[(95,16,12),(133,8,16),(162,12,8),(197,12,8)]
# These four are actual disp8 bytes in MOV instructions, not masked values.
# Capture all operand bytes and both complete bodies in the report.
slots=[{'offset':offset,'role':role,'native_instruction_hex':a[offset-3:offset+1].hex(),
        'candidate_instruction_hex':b[offset-3:offset+1].hex(),'native_disp8':x,'candidate_disp8':y}
       for (offset,x,y),role in zip(suffix,('sequence pointer reload','enable reload','active reload','active clear'))]
assert [(v['native_instruction_hex'],v['candidate_instruction_hex']) for v in slots]==[
 ('8b542410','8b54240c'),('8b4c2408','8b4c2410'),('8b44240c','8b442408'),('8944240c','89442408')]
report={'cells':4,'shared_functions_per_cell':9,'exact_functions_per_cell':3,'changed_symbols':['SGD_sequence_gen','SGD_correlate'],
 'complete_nontext_and_symbols_unchanged':True,'native_sequence_body_hex':a.hex(),'candidate_sequence_body_hex':b.hex(),
 'all_47_raw_differences':diffs,'suffix_start':0x49,'suffix_stack_operand_differences':slots,
 'claim':'After offset0x49 every raw byte agrees except four explicitly listed disp8 bytes. Original slot meanings inferred from original load/store producers and consumers. No normalization or identity claim for the full body.',
 'correlate_before':cells['baseline']['verdicts']['SGD_correlate'],'correlate_after':cells['postdecrement-expression']['verdicts']['SGD_correlate']}
observations=json.loads((d.ROOT/'build/sgd-sequence293-observe/results.json').read_text())
searches=0;pairs={}
for name,cell in observations['cells'].items():
 assert cell['raw_object_unchanged'];validate_trace(cell['trace'],'SGD_correlate')
 events=cell['trace']['events'];searches+=len(events)
 for event in events:assert transfer(event,event['cursor_before'])==(event['selected'],event['cursor_after'])
 pairs[name]=[(e['insn_uid'],e['cursor_before'],e['selected'],e['cursor_after']) for e in events if e['function']=='SGD_correlate']
assert searches==52 and len(pairs)==4
assert pairs['Sgd/postdecrement-expression']==[(125,15,6,16),(56,16,0,49)]
report['observed_scratch_searches_modeled']=searches;report['target_search_pairs']=pairs
(root/'audit.json').write_text(json.dumps(report,indent=2)+'\n')
print('4/4 full-TU metadata/nontext controls pass; exact3/9 unchanged; sequence246B has47 raw differences, only4 disp8 differences after+0x49; correlate BYTES3→BYTES9 explicitly retained; 52/52 native scratch searches modeled')
