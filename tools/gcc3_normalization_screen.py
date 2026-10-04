#!/usr/bin/env python3
"""Screen original backedges for split increments and terminal word stores.

This is an instruction-pattern candidate generator, not proof of memory
promotion, output-helper factoring, alias safety, or a source preimage.
"""
import argparse
import json
import re
from pathlib import Path
import playbook_small_patterns as d
from gcc3_alignment_stack_screen import inventory

LOW={'eax':'ax','ebx':'bx','ecx':'cx','edx':'dx','esi':'si','edi':'di','ebp':'bp'}

def witness(stream):
    found=[]
    for j,(mn,op) in enumerate(stream):
        jump=re.fullmatch(r'insn:(\d+)',op)
        if not mn.startswith('j') or mn=='jmp' or not jump:continue
        first=int(jump[1])
        if first>=j:continue
        for i in range(first,j):
            m,o=stream[i]
            inc=re.fullmatch(r'0x1\(%(e\w+)\),%(e\w+)',o)
            if m!='lea' or not inc:continue
            before,after=inc.groups()
            if before==after or after not in LOW:continue
            copies=[k for k in range(i+1,j) if stream[k]==['mov','%'+after+',%'+before]]
            stores=[k for k in range(j+1,min(j+4,len(stream))) if stream[k][0]=='mov'
                    and re.match('%'+LOW[after]+r',.*\(',stream[k][1])]
            if copies and stores:
                found.append(dict(loop_first=first,increment=i,copy=copies[0],backedge=j,
                                  word_store=stores[0],old_register=before,next_register=after))
    return found

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--baseline-dir',type=Path,default=d.ROOT/'build/production-before')
    p.add_argument('--json',type=Path,default=d.ROOT/'build/normalization-screen.json')
    args=p.parse_args();objects=sorted(args.baseline_dir.glob('*.o'))
    if len(objects)!=300:raise ValueError('expected complete 300-object baseline')
    ref=inventory(Path(d.b.BLOB));positive=ref['FPM_div']['normalized']
    assert len(witness(positive))==1
    bad=[r for r in positive if r!=['mov','%edx,%ecx']]
    assert not witness(bad)
    rows=[];common=eligible=exact=0
    for obj in objects:
        for name,ours in inventory(obj).items():
            if name not in ref:continue
            common+=1;w=witness(ref[name]['normalized'])
            if not w:continue
            eligible+=1;v=d.b.verdict(*d.b.body(d.b.BLOB,name),*d.b.body(str(obj),name))
            if v[0]=='EXACT':exact+=1;continue
            rows.append(dict(object=obj.name,symbol=name,blob_size=ref[name]['size'],
                             ours_size=ours['size'],verdict=v,witness=w,
                             original=ref[name]['normalized']))
    result=dict(objects=len(objects),common_defining_copies=common,eligible=eligible,
                exact_eligible=exact,nonexact_candidates=len(rows),candidates=rows,
                controls=dict(real_div_positive=True,missing_counter_copy_refused=True),
                limitations=__doc__)
    args.json.write_text(json.dumps(result,indent=2)+'\n')
    print(common,'defining copies;',eligible,'eligible;',exact,'already exact;',len(rows),
          'candidates; 1 real positive / 1 missing-copy refusal')
    for row in rows:print(row['object'],row['symbol'],row['verdict'])

if __name__=='__main__':main()
