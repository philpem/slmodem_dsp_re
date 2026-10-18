#!/usr/bin/env python3
"""Model the observed SGD scratch pair; no compiler writes or source variants."""
import json
import copy
from pathlib import Path
import gcc3_mechanism_scratch_observe as observe
from gcc3_alignment_scratch_model import transfer
ROOT=Path(__file__).resolve().parents[1]
source=ROOT/'build/sgd-scratch293-input'
out=ROOT/'build/sgd-scratch293-observe'
result=json.loads((out/'results.json').read_text())
cell=result['cells']['Sgd/baseline'];trace=cell['trace']
observe.validate_trace(trace,'SGD_correlate')
assert cell['raw_object_unchanged']
assert (out/'Sgd-baseline/observed.o').read_bytes()==(source/'Sgd/baseline/candidate.o').read_bytes()
assert (out/'Sgd-baseline/observed.s').read_bytes()==(out/'Sgd-baseline/plain.s').read_bytes()==(source/'Sgd/baseline/Sgd.s').read_bytes()
events=trace['events'];assert len(events)==11
for event in events:
 assert transfer(event,event['cursor_before'])==(event['selected'],event['cursor_after'])
target=[e for e in events if e['function']=='SGD_correlate']
assert len(target)==2
pop,reload=target
assert [(e['insn_uid'],e['cursor_before'],e['selected'],e['cursor_after']) for e in target]==[(125,2,2,3),(56,3,3,13)]
assert pop['eligibility']['live']==[0,7] and reload['eligibility']['live']==[6,7]
assert not pop['excluded'] and not reload['excluded']
assert [(e['register'],e['order_index']) for e in pop['candidates']]==[(2,2)]
assert [(e['register'],e['order_index']) for e in reload['candidates']]==[(7,3),(37,4),(38,5),(39,6),(40,7),(41,8),(42,9),(43,10),(44,11),(3,12)]
def pair(start):
 first,cursor=transfer(pop,start);second,end=transfer(reload,cursor)
 return (first,second,end)
reference_pair_starts=[start for start in range(53) if pair(start)[:2]==(3,4)]
assert reference_pair_starts==list(range(3,13))
predecessor=events[-3]
assert predecessor['function']=='SGD_sequence_gen' and predecessor['insn_uid']==24
predecessor_outputs=sorted({transfer(predecessor,start) for start in range(53)})
assert predecessor_outputs==[(1,2),(3,13)]
predecessor_pairs=[{'predecessor_selected':reg,'cursor':cursor,'target_pair':pair(cursor)} for reg,cursor in predecessor_outputs]
assert all(item['target_pair'][:2]!=(3,4) for item in predecessor_pairs)
# The real missing-target compiler run must finish but the observer must refuse it.
missing=ROOT/'build/sgd-scratch293-absent/Sgd-baseline'
missing_trace=json.loads((missing/'scratch-observe.json').read_text())
assert missing_trace['exit_codes']==[0] and not missing_trace['errors'] and missing_trace['pending']==0
assert (missing/'observed.s').read_bytes()==(source/'Sgd/baseline/Sgd.s').read_bytes()
assert 'target detector did not fire' in (missing/'gdb-scratch.log').read_text()
refusals=[]
try:observe.validate_trace(missing_trace,'SGD_absent_293_control')
except AssertionError:refusals.append('actual-absent-target')
else:raise AssertionError('absent target accepted')
bad=copy.deepcopy(pop);bad['mode']='unsupported'
try:transfer(bad,bad['cursor_before'])
except AssertionError:refusals.append('unsupported-mode')
else:raise AssertionError('unsupported mode accepted')
bad=copy.deepcopy(pop);bad['selected']=3
try:assert transfer(bad,bad['cursor_before'])==(bad['selected'],bad['cursor_after'])
except AssertionError:refusals.append('corrupt-selection')
else:raise AssertionError('corrupt selection accepted')
report={'observed':11,'modeled':11,'raw_assembly_and_object_unchanged':True,'target_searches':target,
 'reference_pair_counterfactual_start_cursors':reference_pair_starts,'all_53_predecessor_start_cursor_outputs':predecessor_outputs,
 'predecessor_outputs_through_target':predecessor_pairs,'refusals':refusals,
 'interpretation':'Current pair is linked by persistent search cursor, with different live sets. Reference BX/SI pair is possible at target starting cursors3..12; varying only earlier cursor cannot produce it with unchanged final predecessor event. Original compiler state is unavailable; this does not establish original source or pass history.',
 'diagnostic_only':True}
(out/'model.json').write_text(json.dumps(report,indent=2)+'\n')
print('11/11 observed SI/general scratch choices modeled; 2/2 target choices traced; 53/53 modeled predecessor cursor starts exclude reference pair; 3/3 negative controls refused; raw assembly/object unchanged')
