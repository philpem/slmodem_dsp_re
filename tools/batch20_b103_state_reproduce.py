#!/usr/bin/env python3
"""Recover original B103 per-case state announcement diagnostics."""
import itertools
import playbook_small_patterns as d
d.REV='93d7eee1';d.SOURCE_PATHS=('src/pump/b103/B103prc.c',);d.OUT_NAME='batch20-b103-state'
def restored(fn):
    parts=fn.split('\n\tcase ')
    new=parts[0]
    for part in parts[1:]:
        state=part.split(':',1)[0]
        marker='\t\tbreak;'
        assert part.count(marker)>=1
        part=part.replace(marker,'\t\tif (DSPLIB_DEBUG_ON())\n\t\t\tdsplibs_debug_printf("'+state+'\\n");\n'+marker,1)
        new+='\n\tcase '+part
    old='\tdefault:\n\t\tbreak;';rep='\tdefault:\n\t\tif (DSPLIB_DEBUG_ON())\n\t\t\tdsplibs_debug_printf("default\\n");\n\t\tbreak;'
    assert new.count(old)==1
    return new.replace(old,rep)
def variants(path,source):
    cells={}
    for org,ans in itertools.product((False,True),repeat=2):
        text=source
        for name,on in [('B103OriginateNextState',org),('B103AnswerNextState',ans)]:
            if on:
                a,b,fn=d.function(text,name);text=text[:a]+restored(fn)+text[b:]
        label='baseline' if not(org or ans) else ('org' if org else '')+('-ans' if ans else '')
        cells[label]=text
    assert len(cells)==len(set(cells.values()))==4
    return cells
d.variants=variants
if __name__=='__main__':d.main()
