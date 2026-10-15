#!/usr/bin/env python3
"""Associate clone dump blocks by emitted labels and printed machine RTL.

Never infer C1/C2 ownership from heading order. Missing labels and duplicate
matches are refusals. No original RTL or allocator-state reconstruction.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
from gcc3_reload_trace import expressions,instructions
from gcc3_stage_divergence import chunks,discover,fingerprint


def labels(text):
    result=set()
    for line in text.splitlines():
        if not re.match(r'^\(code_label(?::\S+)? ',line):continue
        node=expressions(line)[0]
        quote=next(i for i,x in enumerate(node) if isinstance(x,str) and x.startswith('"'))
        number=int(node[quote-1])
        assert number not in result,'duplicate code-label number'
        result.add(number)
    return result


def unique_label_owner(blocks,emitted):
    overlaps=[sorted(labels(block)&emitted) for block in blocks]
    owners=[i for i,values in enumerate(overlaps) if values]
    if len(owners)!=1:raise ValueError('label ownership absent or ambiguous')
    return owners[0],overlaps


def assembly_body(text,symbol):
    start=text.find('\n'+symbol+':')
    if start<0:raise ValueError('assembly symbol absent')
    end=re.search(r'(?m)^\s*\.size\s+'+re.escape(symbol)+r'\s*,',text[start:])
    if end is None:raise ValueError('assembly size boundary absent')
    return text[start:start+end.start()]


def machine_owner(final,emitted,printed):
    matches=[]
    for index,block in enumerate(final):
        actual=instructions(block)
        if all(uid in actual and fingerprint(pattern)==fingerprint(actual[uid]) for uid,pattern in printed.items()):
            matches.append(index)
    if emitted:
        chosen,overlaps=unique_label_owner(final,emitted)
        if chosen not in matches:raise ValueError('label/printed-RTL ownership disagreement')
    else:
        if len(matches)!=1:raise ValueError('printed machine RTL match absent or ambiguous')
        chosen=matches[0]
    return chosen


def trace(folder,stem,header,symbol):
    body=assembly_body((folder/(Path(stem).stem+'.s')).read_text(),symbol)
    emitted={int(x) for x in re.findall(r'^\.L(\d+):',body,re.M)}
    rtl='\n'.join(line[1:] for line in body.splitlines() if line.startswith('#'))
    printed=instructions(rtl)
    final=chunks(folder/(stem+'.35.mach'),header)
    chosen=machine_owner(final,emitted,printed)
    _,stages,excluded=discover(folder,stem)
    records={}
    for stage,path in sorted(stages.items()):
        try:
            blocks=chunks(path,header)
            index,overlaps=unique_label_owner(blocks,emitted)
            records[stage]={'status':'linked','chunk_index':index,'header_copies':len(blocks),
                            'shared_emitted_labels':overlaps[index],
                            'instruction_patterns':{str(uid):pattern for uid,pattern in instructions(blocks[index]).items()}}
        except (ValueError,StopIteration) as error:
            records[stage]={'status':'refused','reason':str(error).split('; available:')[0]}
    if records['35.mach']['status']=='linked':
        assert records['35.mach']['chunk_index']==chosen
    else:
        assert not emitted
        records['35.mach']={'status':'linked','chunk_index':chosen,'header_copies':len(final),
                            'shared_emitted_labels':[], 'proof':'unique printed machine RTL',
                            'instruction_patterns':{str(uid):pattern for uid,pattern in instructions(final[chosen]).items()}}
    inputs=[folder/(Path(stem).stem+'.s'),folder/'candidate.o']+list(stages.values())
    hashes={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}
    return {'symbol':symbol,'header':header,'emitted_labels':sorted(emitted),'input_sha256':hashes,
            'printed_machine_instructions':len(printed),'machine_clone_index':chosen,
            'stages':records,'excluded_stages':excluded}


def self_test():
    a='(code_label:HI 5 4 6 2 11 "" [1 uses])\n'
    b='(code_label 5 4 6 22 "" [1 uses])\n'
    assert unique_label_owner([a,b],{22})[0]==1
    assert unique_label_owner([b,a],{22})[0]==0
    assert labels(a)=={11} and labels(b)=={22}
    refusals=0
    for blocks,witness in [([a,a],{11}),([a,b],{33}),([a,b],set())]:
        try:unique_label_owner(blocks,witness)
        except ValueError:refusals+=1
        else:raise AssertionError('invalid clone association accepted')
    assert refusals==3
    one='(insn 1 0 2 (set (reg:SI 0 ax) (const_int 5 [0x5])) 36 {*movsi_1} (nil) (nil))\n'
    other=one.replace('const_int 5 [0x5]','const_int 6 [0x6]')
    printed=instructions(one)
    assert machine_owner([other,one],set(),printed)==1
    machine_refusals=0
    for blocks,witness in [([one,one],set()),([other],set()),([a+other,b+one],{11})]:
        try:machine_owner(blocks,witness,printed)
        except ValueError:machine_refusals+=1
        else:raise AssertionError('invalid machine association accepted')
    assert machine_refusals==3
    return {'label_formats':2,'order_independent_positives':2,'label_refusals':3,
            'machine_positive':1,'machine_refusals':3}


if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--folder',type=Path,required=True);ap.add_argument('--stem',required=True)
    ap.add_argument('--header',required=True);ap.add_argument('--symbol',required=True)
    ap.add_argument('--output',type=Path,required=True);args=ap.parse_args()
    result=trace(args.folder,args.stem,args.header,args.symbol)
    result['controls']=self_test();args.output.write_text(json.dumps(result,indent=2)+'\n')
    print(args.symbol, sum(r['status']=='linked' for r in result['stages'].values()),'linked stages /',len(result['stages']),result['controls'])
