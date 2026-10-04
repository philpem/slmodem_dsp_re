#!/usr/bin/env python3
"""V23 constructor original diagnostics and observed conditional arm polarity."""
import itertools
import playbook_small_patterns as d
import batch20_v23_composite_reproduce as composite
d.REV='93d7eee1';d.SOURCE_PATHS=('src/pump/v23/v23modem.c',);d.OUT_NAME='batch20-v23-constructor'
def variants(path,source):
    cells={}
    debugsource=composite.variants(path,source)['create-answer']
    a=source.index('\t\tif (m->mode != 0) {');b=source.index('\n\t\t} else {',a);c=source.index('\n\t\t}\n\t}',b)
    host=source[a+len('\t\tif (m->mode != 0) {'):b]
    terminal=source[b+len('\n\t\t} else {'):c]
    old=source[a:c+len('\n\t\t}')]
    new='\t\tif (m->mode == 0) {'+terminal+'\n\t\t} else {'+host+'\n\t\t}'
    for diag,arm,ternary in itertools.product((False,True),repeat=3):
        text=debugsource if diag else source
        if arm:text=text.replace(old,new)
        if ternary:text=text.replace('m->mode != 0 ? V23_ANSWER_TONE_SCALE : 0','m->mode == 0 ? 0 : V23_ANSWER_TONE_SCALE')
        label='baseline' if not(diag or arm or ternary) else ('debug' if diag else '')+('-arm' if arm else '')+('-ternary' if ternary else '')
        cells[label]=text
    assert len(cells)==len(set(cells.values()))==8
    return cells
d.variants=variants
if __name__=='__main__':d.main()
