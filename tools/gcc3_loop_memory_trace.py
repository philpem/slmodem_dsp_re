#!/usr/bin/env python3
"""Report GCC3 loop memory promotion and the emitted shadow-register stores.

This reads the compiler's explicit diagnostics; it does not infer alias safety,
source identity, reachability, or the cause of an original object's registers.
"""
import argparse
import json
import re
from pathlib import Path
from gcc3_reload_trace import expressions, instructions

def walk(node):
    if isinstance(node,list):
        yield node
        for child in node:
            yield from walk(child)

def trace(text, name=None):
    chunks=re.split(r'^;; Function ',text,flags=re.M)[1:]
    if not chunks:
        raise ValueError('no function dump blocks')
    reports=[]
    for chunk in chunks:
        function=chunk.splitlines()[0].split()[0]
        if name is not None and function!=name:
            continue
        patterns=instructions(chunk)
        stream_start=re.search(r'^\((?:insn|jump_insn|call_insn)(?::\S+)? ',chunk,re.M)
        if stream_start is None:
            raise ValueError('function has no instruction stream')
        depth=0;depths={}
        for node in expressions(chunk[stream_start.start():]):
            if node and isinstance(node[0],str) and node[0].startswith('note'):
                if 'NOTE_INSN_LOOP_BEG' in node:depth+=1
                if 'NOTE_INSN_LOOP_END' in node:depth-=1
                if depth<0:raise ValueError('unbalanced loop notes')
            if node and re.fullmatch(r'(?:insn|jump_insn|call_insn)(?::\S+)?',node[0]):
                depths[int(node[1])]=depth
        if depth:raise ValueError('unterminated loop notes')
        events=[]
        for match in re.finditer(r'^Hoisted regno (\d+) (r/w|r/o) from ',chunk,re.M):
            end=chunk.find('\n',match.end())
            raw=chunk[match.end():end if end>=0 else None]
            mem=expressions(raw)
            if len(mem)!=1 or not mem[0][0].startswith('mem:'):
                raise ValueError('unsupported or truncated promoted memory diagnostic')
            pseudo=int(match[1]);memory=mem[0]
            writes=[];shadow_uses=[]
            for uid,pattern in patterns.items():
                for node in walk(pattern):
                    if node and re.match(r'reg(?:/[^:]*)?:',node[0]) and int(node[1])==pseudo:
                        shadow_uses.append(uid)
                    if len(node)==3 and node[0]=='set' and node[1]==memory:
                        writes.append(uid)
            if match[2]=='r/w' and not writes:
                raise ValueError('reported writable promotion without retained memory store')
            if not shadow_uses:
                raise ValueError('reported promotion without shadow register use')
            events.append(dict(pseudo=pseudo,access=match[2],mode=memory[0].split(':')[1],
                               memory=memory,store_uids=sorted(set(writes)),
                               store_loop_depths={uid:depths[uid] for uid in sorted(set(writes))},
                               shadow_use_uids=sorted(set(shadow_uses))))
        reports.append(dict(function=function,instructions=len(patterns),promotions=events))
    if name is not None and len(reports)!=1:
        raise ValueError('requested function does not occur exactly once')
    return dict(functions=len(reports),instructions=sum(r['instructions'] for r in reports),
                promotions=sum(len(r['promotions']) for r in reports),reports=reports)

def self_test():
    fixture=''';; Function sample
Hoisted regno 94 r/w from (mem:HI (reg/v/f:SI 70 [ count ]) [3 S2 A16])
(insn 1 0 2 (set (reg/v:HI 94) (const_int 0)) -1 (nil) (nil))
(insn 2 1 0 (set (mem:HI (reg/v/f:SI 70 [ count ]) [3 S2 A16]) (reg/v:HI 94)) -1 (nil) (nil))
'''
    positive=trace(fixture,'sample');assert positive['promotions']==1
    assert positive['reports'][0]['promotions'][0]['store_uids']==[2]
    without=fixture.replace(fixture.splitlines()[1]+'\n','')
    assert trace(without,'sample')['functions']==1 and trace(without,'sample')['promotions']==0
    for broken in [fixture.replace('(mem:HI (reg/v/f:SI 70 [ count ]) [3 S2 A16])\n','(mem:HI\n',1),
                   fixture.replace('(mem:HI (reg/v/f:SI 70 [ count ]) [3 S2 A16]) (reg/v:HI 94)',
                                   '(reg/v:HI 95) (reg/v:HI 94)'),
                   fixture.replace('Function sample','Function other')]:
        try:trace(broken,'sample')
        except ValueError:pass
        else:raise AssertionError('invalid diagnostic accepted')
    print('loop memory trace: 5 controls passed (1 writable positive, 1 no-promotion negative, 3 refusals)')

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('dump',nargs='?',type=Path);parser.add_argument('--function')
    parser.add_argument('--json',type=Path);parser.add_argument('--self-test',action='store_true')
    args=parser.parse_args()
    if args.self_test:self_test()
    if args.dump is None:
        if args.self_test:return
        parser.error('a loop dump is required')
    result=trace(args.dump.read_text(),args.function)
    if args.json:args.json.write_text(json.dumps(result,indent=2)+'\n')
    print('loop memory trace:',result['functions'],'functions;',result['instructions'],
          'instructions;',result['promotions'],'explicit promotions')

if __name__=='__main__':main()
