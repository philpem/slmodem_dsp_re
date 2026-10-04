#!/usr/bin/env python3
"""The complete blob-supported V23 wrapper emission order and entry site."""
import itertools
import playbook_small_patterns as d
import batch20_v23_debug_reproduce as debug
d.REV='93d7eee1';d.SOURCE_PATHS=('src/pump/v23/v23.c',);d.OUT_NAME='batch20-v23-order'
names=['v23_create','v23_delete','v23_process','dp_v23_init','dp_v23_exit']
def reordered(text):
    spans=[];defs={}
    for name in names:
        a,b,fn=d.function(text,name)
        a=text.rfind('\n',0,a-1)+1
        defs[name]=text[a:b];spans.append((a,b))
    for a,b in sorted(spans,reverse=True):text=text[:a]+text[b:]
    return text+'\n\n'+'\n\n'.join(defs[n] for n in names)+'\n'
def variants(path,source):
    cells={}
    for order,entry in itertools.product((False,True),repeat=2):
        text=debug.variants(path,source)['debug'] if entry else source
        if order:text=reordered(text)
        label='baseline' if not(order or entry) else ('order' if order else '')+('-entry' if entry else '')
        cells[label]=text
    assert len(cells)==len(set(cells.values()))==4
    return cells
d.variants=variants
if __name__=='__main__':d.main()
