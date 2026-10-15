#!/usr/bin/env python3
"""Nominate stack-home exchanges in full bodies or matching local SIZE regions.

This is triage only. Exact grading remains byteident's complete-body verdict.
Relocations use the shared ELF proof; unknown tables are never ignored.
"""
import argparse
from collections import defaultdict, Counter
from difflib import SequenceMatcher
import hashlib
import json
import re
import playbook_small_patterns as d
from residual_spill_screen import MEM, witness

RANK = {'EXACT':0,'UNRESOLVED':1,'REGALLOC':2,'RELOC':3,'BYTES':4,'SIZE':5,'NODATA':6}


def local_windows(left, right):
    keys = lambda rows:[(m,MEM.sub('STACK(%esp)',o)) for m,o in rows]
    blocks = SequenceMatcher(None,keys(left),keys(right),autojunk=False).get_matching_blocks()
    found=[]
    for block in blocks:
        if block.size<8: continue
        a=left[block.a:block.a+block.size]; b=right[block.b:block.b+block.size]
        proof=witness(a,b)
        if not proof or proof['save_area_only']:continue
        changed={x:y for x,y in proof['mapping'].items() if x!=y}
        # Require an actual local permutation, rather than all homes moving
        # together because the candidate allocated a different frame size.
        if len(changed)<2 or set(changed)!=set(changed.values()):continue
        found.append({'original_row':block.a,'candidate_row':block.b,
                      'row_count':block.size,**proof})
    return found


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--output',default='build/residual-stack-windows.json')
    args=ap.parse_args()
    assert not d.b._staleness()[0], 'run make tc'
    objects=defaultdict(list); sources={}
    blob=d.b.sizes(d.b.BLOB)
    for line in (d.ROOT/'build/tc_out/tc_manifest.txt').read_text().splitlines():
        obj,source=line.split();sources[obj]=source
        for symbol in d.b.sizes(str(d.ROOT/'build/tc_out'/obj)):
            if symbol in blob:objects[symbol].append(obj)
    counts=Counter();nominations=[]
    for symbol,copies in sorted(objects.items()):
        original=d.b.body(d.b.BLOB,symbol)
        scored=[(d.b.verdict(*original,*d.b.body(str(d.ROOT/'build/tc_out'/obj),symbol)),obj)
                for obj in copies]
        (grade,delta),obj=max(scored,key=lambda row:RANK[row[0][0]])
        if grade=='EXACT':counts['exact_excluded']+=1;continue
        counts['nonexact_symbols']+=1;counts[grade]+=1
        if grade in ('UNRESOLVED','NODATA','RELOC'):continue
        path=d.ROOT/'build/tc_out'/obj
        left=d.b.insns(d.b.BLOB,symbol);right=d.b.insns(str(path),symbol)
        whole=witness(left,right)
        windows=local_windows(left,right) if grade=='SIZE' else []
        if whole and not whole['save_area_only'] or windows:
            nominations.append({'symbol':symbol,'source':sources[obj],'object':obj,
                'object_sha256':hashlib.sha256(path.read_bytes()).hexdigest(),
                'strict_verdict':[grade,delta],'whole_body':whole,'SIZE_windows':windows})
    # Live historical positive still has opposite double homes and an audited
    # table. Zero scratch/same-size claims are never inferred from this result.
    name='_ZN21V90ConstellationPower8getPowerEP16V90MappingParams26V90TxPowerMeasurementPoint7PcmType'
    original=d.b.insns(d.b.BLOB,name)
    baseline=d.b.insns(str(d.ROOT/'build/gcc3-getpower-lifetime/V90ConstellationPower/baseline/candidate.o'),name)
    positive=witness(original,baseline)
    assert positive and len(positive['changed_stack_rows'])==10 and not positive['save_area_only']
    assert witness(original,original) is None
    altered=list(baseline);i=next(i for i,(m,o) in enumerate(altered) if m=='jmp' and 'jump-table' in o)
    altered[i]=(altered[i][0],altered[i][1]+'CHANGED-TARGET')
    assert witness(original,altered) is None
    assert local_windows(original,original)==[]
    assert len(objects)==1852
    result={'denominator':dict(counts),'shared_symbols':len(objects),'controls':4,
            'nominations':nominations,'limits':'Exact-register local matches only; SIZE windows are not whole-function or CFG identity. Unknown relocations and all real operands stay visible.'}
    (d.ROOT/args.output).write_text(json.dumps(result,indent=2)+'\n')
    print(dict(counts),len(nominations),'nominations; 4 controls fire')
    for r in nominations:print(r['strict_verdict'],r['symbol'],'whole' if r['whole_body'] else str(len(r['SIZE_windows']))+' SIZE windows')


if __name__=='__main__':main()
