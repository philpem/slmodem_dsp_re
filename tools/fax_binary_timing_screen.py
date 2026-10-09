#!/usr/bin/env python3
"""Coarse fax binary read/write/call and tail screen, never a dataflow proof."""
import argparse,hashlib,json,re
from pathlib import Path
import playbook_small_patterns as d
from gcc3_alignment_stack_screen import inventory
MEM=re.compile(r'(?:-?0x[0-9a-f]+)?\(%e\w+(?:,%e\w+,\d+)?\)')
def features(row):
    events=[];epoch=0
    for index,(mn,ops) in enumerate(row['original']):
        if mn=='call':
            events.append({'index':index,'kind':'call','epoch':epoch,'operands':ops});epoch+=1
        if mn.startswith('j') or mn=='ret':
            events.append({'index':index,'kind':'branch' if mn!='ret' else 'return','epoch':epoch,'opcode':mn,'operands':ops})
        if mn=='lea':continue
        matches=list(MEM.finditer(ops))
        for match in matches:
            token=match.group()
            if '%esp' in token:continue
            kind='read'
            if ops.endswith(token) and mn.startswith(('mov','pop','fst')):kind='write'
            elif ops.endswith(token) and mn.startswith(('add','sub','and','or','xor','inc','dec')):kind='read-write'
            events.append({'index':index,'kind':kind,'epoch':epoch,'opcode':mn,'operands':ops,'memory':token})
    return {'returns':sum(e['kind']=='return' for e in events),'reads':sum(e['kind'] in ('read','read-write') for e in events),'writes':sum(e['kind'] in ('write','read-write') for e in events),'calls':epoch,'events':events}
def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--baseline-dir',type=Path,required=True)
    ap.add_argument('--census',type=Path,required=True)
    ap.add_argument('--output',type=Path,default=d.ROOT/'build/fax-binary-timing-screen.json')
    args=ap.parse_args();ref=inventory(Path(d.b.BLOB));exact=set(json.loads(args.census.read_text())['exact_symbols'])
    rows=[];shared=eligible=objects=0
    for line in (args.baseline_dir/'tc_manifest.txt').read_text().splitlines():
        name,source=line.split()
        if not source.startswith('src/fax/'):continue
        objects+=1;obj=args.baseline_dir/name;ours=inventory(obj)
        for symbol,right in ours.items():
            if symbol not in ref:continue
            shared+=1;left=ref[symbol]
            if left['size']>350 or symbol in exact:continue
            eligible+=1;a,z=features(left),features(right)
            rows.append({'symbol':symbol,'source':source,'object':name,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'bytes':[left['size'],right['size']],'original':a,'current':z,'delta':{key:a[key]-z[key] for key in ['reads','writes','calls','returns']},'canonical_verdict':d.b.verdict(*d.b.body(d.b.BLOB,symbol),*d.b.body(obj,symbol))})
    witness=next(row for row in rows if row['symbol']=='FSE_decision_eqtrn')
    assert witness['original']['returns']==2 and witness['current']['returns']==1
    # Instruction-list refusal: the criterion cannot fire without extra RET.
    refused=dict(ref['FSE_decision_eqtrn'])
    code=list(refused['original']);index=next(i for i,row in enumerate(code) if row[0]=='ret')
    refused['original']=code[:index]+code[index+1:]
    assert features(refused)['returns']==witness['current']['returns']
    rows.sort(key=lambda row:(-row['delta']['returns'],-row['delta']['writes'],-row['delta']['reads'],row['bytes'][0]))
    out={'objects':objects,'shared_symbols':shared,'nonexact_eligible_symbols':eligible,'original_byte_limit':350,'census_sha256':hashlib.sha256(args.census.read_bytes()).hexdigest(),'baseline_config':(args.baseline_dir/'.build-config').read_text(),'known_duplicate_tail_detected':True,'missing_extra_return_refused':True,'limitations':'Coarse disassembly events: ESP excluded, EBP retained as possible data base; no base/value/path/alias equivalence or allocation causality. Counts only rank manual inspection. All eligible rows retained, not merely feature hits.','rows':rows}
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(out,indent=2)+'\n')
    print(objects,'fax objects;',shared,'shared functions;',eligible,'nonexact <=350 B; one original duplicate-tail positive and one refusal')
if __name__=='__main__':main()
