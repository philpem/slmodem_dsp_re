#!/usr/bin/env python3
from pathlib import Path
import hashlib,json,collections
HERE=Path(__file__).resolve().parent
x=json.loads((HERE/'audit.json').read_text())
def traces(side):return {s['offset']:s for p in x['paths'] for s in p[side+'_trace']}
nt,ct=traces('native'),traces('candidate')
def lanes(step):
    if step is None:return 'unreachable'
    return '; '.join((w.get('register',w.get('flag','memory'))+': '+','.join(w.get('live_defined_lanes',[]))) for w in step['writes'] if w.get('live_defined_lanes')) or 'no live register/flag definition'
lines=['# Complete instruction mapping','',
       'All addresses are absolute ELF section addresses. The mapping is bijective over 97 decoded instructions. The 95 reachable instructions are checked on every path that executes them; the two unreachable padding instructions are literal identical. `audit.json` retains per-byte definitions, uses, memory addresses/values, flag expressions and before/after liveness for every executed instruction. Register byte 0 is the low byte. Liveness here observes the void ABI, actual memory accesses and call arguments; caller-saved return words have additional equality checks but are not ABI requirements.','',
       '| Native | Candidate | Native instruction | Candidate instruction | Native live definitions | Candidate live definitions |',
       '| --- | --- | --- | --- | --- | --- |']
for row in x['mapping']:
    n,c=row['native'],row['candidate'];fmt=lambda r:f"`{r['mnemonic']} {r['operands']}`"
    lines.append(f"| `0x{n['address']:x}` / `+0x{n['offset']:03x}` | `0x{c['address']:x}` / `+0x{c['offset']:03x}` | {fmt(n)} | {fmt(c)} | {lanes(nt.get(n['offset']))} | {lanes(ct.get(c['offset']))} |")
(HERE/'INSTRUCTION-MAP.md').write_text('\n'.join(lines)+'\n')
summary={}
for side in ['native','candidate']:
    ts=traces(side)
    summary[side]={'reachable_static_instructions':len(ts),'read_events_by_kind':dict(collections.Counter(r['kind'] for s in ts.values() for r in s['reads'])),'write_events_by_kind':dict(collections.Counter(w['kind'] for s in ts.values() for w in s['writes'])),'register_byte_definitions':sum(len(w['bytes']) for s in ts.values() for w in s['writes'] if w['kind']=='register'),'memory_store_bytes':sum(w['size'] for s in ts.values() for w in s['writes'] if w['kind']=='memory')}
summary['frame_cells']=[{'current_sp_offset':0,'bytes':4,'role':'outgoing argument 0; callee permitted to alter'}, {'current_sp_offset':4,'bytes':4,'role':'outgoing argument 1; callee permitted to alter'}, {'current_sp_offset':8,'bytes':3,'role':'unused private padding'}, {'current_sp_offset':11,'bytes':1,'role':'saved incoming late byte'}, {'current_sp_offset':12,'bytes':4,'role':'saved entry EBX'}, {'current_sp_offset':16,'bytes':4,'role':'saved entry ESI'}, {'current_sp_offset':20,'bytes':4,'role':'saved entry EDI'}, {'current_sp_offset':24,'bytes':4,'role':'saved entry EBP'}, {'current_sp_offset':28,'bytes':4,'role':'incoming return target'}, {'current_sp_offset':32,'bytes':4,'role':'object formal slot'}, {'current_sp_offset':36,'bytes':4,'role':'message formal slot'}, {'current_sp_offset':40,'bytes':4,'role':'late formal slot; only byte 0 consumed'}, {'current_sp_offset':44,'bytes':4,'role':'flag formal slot; only byte 0 consumed'}]
summary['flags']=[{'offset':0x42,'consumer':0x4b,'width':32,'semantics':'message == 1','live_flags':['ZF']},{'offset':0x160,'consumer':0x167,'width':32,'semantics':'unsigned debug > 1','live_flags':['CF','ZF']},{'offset':0x6d,'consumer':0x6f,'width':8,'semantics':'late != 0','live_flags':['ZF']},{'offset':0x11b,'consumer':0x141,'width':16,'semantics':'role == 0x65','live_flags':['ZF']},{'offset':0x143,'consumer':0x147,'width':16,'semantics':'role == 0x66','live_flags':['ZF']}]
summary['proof_results']=dict(collections.Counter(p['result'] for p in x['proofs']))
summary['call_liveness']=[{'target':s['call_target'],'native_current_live_lanes_after':s['lane_liveness_after'],'candidate_current_live_lanes_after':ct[s['offset']]['lane_liveness_after']} for s in x['paths'][0]['native_trace'] if 'call_target' in s]
(HERE/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
print(json.dumps(summary,indent=2))
