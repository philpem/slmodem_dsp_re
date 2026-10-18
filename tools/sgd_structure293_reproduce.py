#!/usr/bin/env python3
"""Bounded SGD correlate width/countdown/source-operand cross for PR293."""
import itertools
import playbook_small_patterns as d

def variants(path, source):
    a,z,fn=d.function(source,'SGD_correlate')
    cells={}
    for narrow,increment,xor in itertools.product((False,True),repeat=3):
        f=fn
        if narrow:f=f.replace('int scale =','short scale =')
        if increment:f=f.replace('for (i = (short)(n - 1); i != -1; i = (short)(i - 1))','for (i = n; i-- != 0;)')
        if xor:f=f.replace('*b++ ^ *a++','*a++ ^ *b++')
        label='baseline' if not any((narrow,increment,xor)) else '-'.join(k for k,v in [('short-scale',narrow),('increment-test',increment),('a-first',xor)] if v)
        cells[label]=source[:a]+f+source[z:]
    assert len(cells)==len(set(cells.values()))==8
    return cells

d.REV='96ce7c20d'
d.OUT_NAME='sgd-countdown293'
d.SOURCE_PATHS=('src/fax/Sgd.c',)
d.variants=variants
d.main()
