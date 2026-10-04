#!/usr/bin/env python3
"""Replay calling-tone clamp, branch-arm and call-load lifetime controls."""
import argparse
import itertools
import sys
from pathlib import Path
import playbook_small_patterns as driver


def variants(path,source):
    start,end,fn=driver.function(source,'GenerateCallingTone')
    clamp='end = remaining < count ? remaining : count;'
    begin=fn.index('\t\tif (ct->on == 0) {')
    middle=fn.index('\t\t} else {',begin)
    finish=fn.index('\n\t\tremaining -= end;',middle)
    zero=fn[begin+len('\t\tif (ct->on == 0) {'):middle]
    on=fn[middle+len('\t\t} else {'):finish]
    assert on.endswith('\n\t\t}\n')
    on=on[:-len('\n\t\t}\n')]
    old=fn[begin:finish]
    swapped='\t\tif (ct->on != 0) {'+on+'\n\t\t} else {'+zero+'\n\t\t}\n'
    cells={}
    for statement,onfirst,preload in itertools.product((False,True),repeat=3):
        text=fn
        if statement:text=text.replace(clamp,'end = remaining;\n\t\tif (end > count)\n\t\t\tend = count;')
        if onfirst:text=text.replace(old,swapped)
        if preload:
            text=text.replace('\t\t\t\tshort v;','\t\t\t\tshort v;\n\t\t\t\tint amplitude = ct->amplitude;')
            text=text.replace('(ct->amplitude * v)','(amplitude * v)')
        label='-'.join(n for n,v in [('statement-clamp',statement),('on-first',onfirst),('preload',preload)] if v) or 'baseline'
        cells[label]=source[:start]+text+source[end:]
    assert len(cells)==len(set(cells.values()))==8
    if TAIL:
        selected=cells['statement-clamp-on-first-preload']
        tail_start=selected.index('\t\tif (remaining != 0) {')
        tail_end=selected.index('\n\t}\n}',tail_start)
        transition=selected[selected.index('\t\tif (ct->on != 0) {',tail_start):tail_end]
        explicit='\t\tif (remaining == 0) {\n'+''.join('\t'+line+'\n' for line in transition.splitlines())+'\t\t} else {\n\t\t\tct->remaining = remaining;\n\t\t}'
        product='buf[i] = (short)((amplitude * v) >> 13);'
        inplace='amplitude *= v;\n\t\t\t\tamplitude >>= 13;\n\t\t\t\tbuf[i] = (short)amplitude;'
        assert selected.count(product)==1
        cells={'baseline':source,'combined':selected,
               'in-place-amplitude':selected.replace(product,inplace),
               'period-if-else':selected[:tail_start]+explicit+selected[tail_end:]}
        cells['period-if-else-in-place-amplitude']=cells['period-if-else'].replace(product,inplace)
        assert len(cells)==len(set(cells.values()))==5
    return cells

if __name__=='__main__':
    parser=argparse.ArgumentParser(add_help=False)
    parser.add_argument('--tail',action='store_true')
    opts,remaining=parser.parse_known_args()
    TAIL=opts.tail
    sys.argv=sys.argv[:1]+remaining
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
    driver.REV='80c5dea3'
    driver.OUT_NAME='gcc3-batch5-calling-tone'+('-tail' if TAIL else '')
    driver.SOURCE_PATHS=('src/callprog/CallingTone.c',)
    driver.variants=variants
    driver.main()
