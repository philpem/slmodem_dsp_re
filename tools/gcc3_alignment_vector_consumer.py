#!/usr/bin/env python3
"""Bounded direct-index transfer to the original two V90 pump witnesses."""
import itertools
import playbook_small_patterns as d
from gcc3_alignment_vector_split import variants as split

def variants(path,source):
    cells={}
    for leaves,consumer in itertools.product((False,True),repeat=2):
        text=split(path,source)['phase-1-v92-1'] if leaves else source
        if consumer:
            start,end,fn=d.function(text,'V90Phase3Modulator::generateV90Symbol')
            old='vectorBit(jdBits, symbolCount)';assert fn.count(old)==2
            text=text[:start]+fn.replace(old,'jdBits[(symbolCount - 1u) % 72u]')+text[end:]
        label='baseline' if not(leaves or consumer) else 'leaves-%d-consumer-%d'%(leaves,consumer)
        cells[label]=text
    return cells
if __name__=='__main__':
    d.REV='2001434c';d.SOURCE_PATHS=('src/pump/v90/V90Phase3Modulator.cpp',)
    d.OUT_NAME='gcc3-alignment-vector-consumer';d.DUMP_FLAGS=('-v','-save-temps','-da')
    d.variants=variants;d.main()
