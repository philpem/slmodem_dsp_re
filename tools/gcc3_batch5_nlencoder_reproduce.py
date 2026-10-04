#!/usr/bin/env python3
"""Replay a bounded nonlinear-encoder input/arithmetic lifetime cross."""
import itertools
import sys
from pathlib import Path
import playbook_small_patterns as driver


def variants(path, source):
    start,end,fn=driver.function(source,'V34nlencoder')
    square='mag = (re * re + im * im + 0x800) >> 12;'
    gain='g = (short)(mag + ((t * 0x4ccd) >> 16) + 0x4000);'
    assert fn.count(square)==fn.count(gain)==1
    cells={}
    for reverse,update in itertools.product((False,True),repeat=2):
        text=fn
        if reverse:text=text.replace(square,'mag = (im * im + re * re + 0x800) >> 12;')
        if update:text=text.replace(gain,'mag += ((t * 0x4ccd) >> 16) + 0x4000;\n\tg = (short)mag;')
        label='-'.join(n for n,v in [('reverse-squares',reverse),('update-mag',update)] if v) or 'baseline'
        cells[label]=source[:start]+text+source[end:]
    assert len(cells)==len(set(cells.values()))==4
    return cells

if __name__=='__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
    driver.REV='80c5dea3'
    driver.OUT_NAME='gcc3-batch5-nlencoder'
    driver.SOURCE_PATHS=('src/pump/v34/V34TX.c',)
    driver.variants=variants
    driver.main()
