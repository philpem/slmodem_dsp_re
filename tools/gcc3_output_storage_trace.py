#!/usr/bin/env python3
"""Read initial output homes and annotated HI stack-load machine selections.

This reports compiler evidence, not simulated allocation or recovered source.
Only direct HI register-from-HI-stack-memory movhi selections are classified.
"""
import argparse
import json
import re
from pathlib import Path
from gcc3_reload_trace import function, instructions


def walk(node):
    if isinstance(node,list):
        yield node
        for child in node:
            yield from walk(child)


def trace(initial,final,assembly,name):
    first=instructions(function(initial,name));last=instructions(function(final,name))
    homes=[]
    for uid,pattern in first.items():
        for node in walk(pattern):
            if len(node)!=3 or node[0]!='set':continue
            dst,src=node[1:]
            if not isinstance(dst,list) or not dst[0].startswith('reg/v/f:SI'):continue
            match=re.search(r'\[ (mantissa|count) \]',' '.join(map(str,dst)))
            if not match or not isinstance(src,list) or src[0]!='plus:SI':continue
            if 'virtual-stack-vars' not in str(src[1]):continue
            if src[2][0]!='const_int':raise ValueError('unsupported output home')
            homes.append(dict(uid=uid,role=match[1],pseudo=int(dst[1]),offset=int(src[2][1])))
    if len({r['role'] for r in homes})!=len(homes):raise ValueError('duplicate output homes')
    lines=assembly.splitlines();active=False;loads=[];selected=0
    for line in lines:
        if line.strip()==name+':':active=True;continue
        if active and re.match(r'\s*\.size\s+'+re.escape(name)+r',',line):break
        if not active:continue
        m=re.match(r'\s*(movl|movzwl|movw)\s+([^#]+)#\s*(\d+)\s+\*movhi_1/3\b',line)
        if not m:continue
        selected+=1;uid=int(m[3])
        if uid not in last:raise ValueError('annotated UID absent from final dump')
        sets=[n for n in walk(last[uid]) if len(n)==3 and n[0]=='set']
        if len(sets)!=1:raise ValueError('ambiguous move pattern')
        dst,src=sets[0][1:]
        if not isinstance(src,list) or not src[0].startswith('mem'):raise ValueError('load lacks memory source')
        addr=src[1]
        if 'sp' not in str(addr):continue
        if dst[0].split(':')[-1]!='HI' or src[0].split(':')[-1]!='HI':
            raise ValueError('annotated movhi is not a direct HI load')
        if addr[0]=='plus:SI' and addr[2][0]=='const_int':offset=int(addr[2][1])
        elif addr[0]=='reg/f:SI' and 'sp' in addr:offset=0
        else:raise ValueError('unsupported stack address')
        operand=re.match(r'\s*(-?\d+)?\(%esp\),\s*(%\w+)\s*$',m[2])
        if not operand or int(operand[1] or 0)!=offset:raise ValueError('assembly/RTL address disagreement')
        loads.append(dict(uid=uid,rtl_mode='HI',stack_offset=offset,
                          offset_mod4=offset%4,mnemonic=m[1],destination=operand[2],
                          pattern='*movhi_1/3'))
    if not active:raise ValueError('function absent from assembly')
    return dict(function=name,initial_instructions=len(first),final_instructions=len(last),
                output_homes=homes,annotated_movhi_loads=selected,HI_stack_loads=loads)


def self_test():
    initial=''';; Function probe
(insn 1 0 0 (set (reg/v/f:SI 68 [ mantissa ]) (plus:SI (reg/f:SI 54 virtual-stack-vars) (const_int -2))) -1 (nil) (nil))
'''
    def final(offset,mode='HI'):
        return ';; Function probe\n(insn 2 0 0 (set (reg:'+mode+' 1 dx) (mem:'+mode+' (plus:SI (reg/f:SI 7 sp) (const_int '+str(offset)+')))) 40 (nil) (nil))\n'
    def asm(offset,mn='movl',uid=2):
        return 'probe:\n\t'+mn+' '+str(offset)+'(%esp), %edx # '+str(uid)+' *movhi_1/3\n.size probe, .-probe\n'
    assert trace(initial,final(16),asm(16),'probe')['HI_stack_loads'][0]['mnemonic']=='movl'
    assert trace(initial,final(18),asm(18,'movzwl'),'probe')['HI_stack_loads'][0]['offset_mod4']==2
    for action in [lambda:trace(initial,final(16,'SI'),asm(16),'probe'),
                   lambda:trace(initial,final(16),asm(18),'probe'),
                   lambda:trace(initial,final(16),asm(16,uid=999),'probe')]:
        try:action()
        except ValueError:pass
        else:raise AssertionError('invalid trace accepted')
    print('output storage trace: 5 controls pass (aligned/unaligned HI positives, SI/address/UID refusals)')


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--initial',type=Path);p.add_argument('--final',type=Path)
    p.add_argument('--assembly',type=Path);p.add_argument('--function');p.add_argument('--json',type=Path)
    p.add_argument('--self-test',action='store_true');args=p.parse_args()
    if args.self_test:self_test()
    if args.initial is None:
        if args.self_test:return
        p.error('--initial, --final, --assembly and --function are required')
    if not all([args.final,args.assembly,args.function]):p.error('incomplete trace inputs')
    result=trace(args.initial.read_text(),args.final.read_text(),args.assembly.read_text(),args.function)
    if args.json:args.json.write_text(json.dumps(result,indent=2)+'\n')
    print(result['initial_instructions'],'initial instructions;',result['final_instructions'],
          'final instructions;',len(result['output_homes']),'output homes;',
          len(result['HI_stack_loads']),'direct HI stack-load selections')
    print(json.dumps(result,indent=2))

if __name__=='__main__':main()
