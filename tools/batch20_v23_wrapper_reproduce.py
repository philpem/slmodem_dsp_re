#!/usr/bin/env python3
"""V23 entry diagnostic and independent id/modem assignment boundary."""
import playbook_small_patterns as d
import batch20_v23_debug_reproduce as debug
d.REV='93d7eee1';d.SOURCE_PATHS=('src/pump/v23/v23.c',);d.OUT_NAME='batch20-v23-wrapper'
def variants(path,source):
    cells={}; old='\tdp->dp.id = id;\n\tdp->dp.modem = modem;';new='\tdp->dp.modem = modem;\n\tdp->dp.id = id;'
    assert source.count(old)==1
    for diag in (False,True):
        for order in (False,True):
            text=debug.variants(path,source)['debug'] if diag else source
            if order:text=text.replace(old,new)
            label='baseline' if not(diag or order) else ('debug' if diag else '')+('-member' if order else '')
            cells[label]=text
    assert len(cells)==len(set(cells.values()))==4
    return cells
d.variants=variants
if __name__=='__main__':d.main()
