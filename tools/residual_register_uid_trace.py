#!/usr/bin/env python3
"""Link register-only differing instruction rows to audited assembly UIDs.

Require one-to-one annotated instruction order and opcode agreement; refuse
ambiguous clone headers or unannotated/multi-instruction expansions. No blob
RTL or original allocator state is reconstructed.
"""
import argparse
from collections import Counter
import json
from pathlib import Path
import re
import playbook_small_patterns as d
from gcc3_stage_divergence import chunks, normalize
from gcc3_reload_trace import instructions


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--inventory', type=Path, required=True)
    ap.add_argument('--audit', type=Path, required=True)
    ap.add_argument('--output', type=Path, required=True)
    args = ap.parse_args()
    inventory = json.loads(args.inventory.read_text())
    audit = json.loads(args.audit.read_text())
    assert inventory['revision'] == audit['revision']
    indexed = {r['symbol']:r for r in audit['targets']}
    rows, counts = [], Counter()
    for target in inventory['candidates']:
        if target['grade'] != 'REGALLOC':
            continue
        proof = indexed[target['symbol']]
        row = {key:target[key] for key in ('symbol','source','demangled')}
        row['status'] = 'unavailable'
        if proof['mapping'] != 'unique header':
            row['reason'] = proof['mapping']; rows.append(row); counts[row['status']] += 1; continue
        folder = Path(proof['baseline_directory'])
        assembly = folder/(Path(target['source']).stem+'.s')
        text = assembly.read_text()
        name = target['symbol']
        start = text.find('\n'+name+':')
        end = re.search(r'(?m)^\s*\.size\s+'+re.escape(name)+r'\s*,', text[start:]) if start >= 0 else None
        if end is None:
            row['reason']='assembly symbol boundary absent'; rows.append(row); counts[row['status']]+=1;continue
        body = text[start:start+end.start()]
        annotated = []
        for line in body.splitlines():
            m = re.match(r'^\s*([a-z][a-z0-9;]*)\s+([^#]*?)\s*#\s*(\d+)\s+',line)
            if m:annotated.append({'mnemonic':m[1],'operands':m[2].strip(),'uid':int(m[3])})
        original = [r for r in target['original_instructions'] if not d.b._padding(*r)]
        retained = [r for r in target['retained_instructions'] if not d.b._padding(*r)]
        if len(annotated)!=len(retained) or len(original)!=len(retained):
            row['reason']='instruction cardinality is not one-to-one'
            row['counts']=[len(original),len(retained),len(annotated)]
            rows.append(row);counts[row['status']]+=1;continue
        compatible = lambda a,b:a==b or (a[-1:] in ('b','w','l') and a[:-1]==b) or (a=='sall' and b=='shl')
        if not all(compatible(a['mnemonic'],b[0]) for a,b in zip(annotated,retained)):
            row['reason']='annotated opcode order does not match object'
            rows.append(row);counts[row['status']]+=1;continue
        header=proof['matched_headers'][0]
        streams={}
        for stage in ('24.lreg','25.greg','27.flow2','28.peephole2','30.rnreg','33.sched2'):
            path=folder/(Path(target['source']).name+'.'+stage)
            if path.exists():streams[stage]=instructions(chunks(path,header)[0])
        traces=[]
        for index,(a,b,annotation) in enumerate(zip(original,retained,annotated)):
            if a==b:continue
            uid=annotation['uid']
            traces.append({'row':index,'original':a,'retained':b,'uid':uid,
                           'retained_patterns':{stage:normalize(stream.get(uid)) for stage,stream in streams.items()}})
        assert traces, 'register-only positive has no differing row'
        row['status']='linked';row['differing_rows']=traces
        rows.append(row);counts[row['status']]+=1
    assert len(rows)==31
    fir=next(r for r in rows if r['symbol']=='_ZN8FloatFIR5resetEv')
    assert fir['status']=='linked'
    assert any(t['retained_patterns']['27.flow2'] is None and t['retained_patterns']['28.peephole2'] is not None for t in fir['differing_rows'])
    args.output.write_text(json.dumps({'revision':inventory['revision'],'counts':dict(counts),'targets':rows,
                                     'scope':'retained UID provenance; no original cursor, lifetime or source syntax inferred'},indent=2)+'\n')
    print('31 register-only targets;',dict(counts))
    for row in rows:print(row['demangled'],row['status'],len(row.get('differing_rows',[])),row.get('reason',''))


if __name__=='__main__':main()
