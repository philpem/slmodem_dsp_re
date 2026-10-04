#!/usr/bin/env python3
"""Bounded V8 detector source-boundary reproduction."""
import sys
from pathlib import Path
import itertools
import playbook_small_patterns as driver

def variants(path,source):
    cells={'baseline':source}
    start,end,fn=driver.function(source,'v8_phase_rev_init')
    forms={
      'phase-field-bound':fn.replace('i < 64','i < pr->half * 2'),
      'phase-cached-bound':fn.replace('\tint i;','\tint i, full;').replace('\tfor (i = 0;', '\tfull = pr->half * 2;\n\tfor (i = 0;').replace('i < 64','i < full'),
      'phase-short-index':fn.replace('\tint i;','\tshort i;'),
    }
    cells.update((label,source[:start]+body+source[end:]) for label,body in forms.items())
    start,end,fn=driver.function(source,'biquad_filter')
    for interleaved, saved in itertools.product((False,True),repeat=2):
      if not interleaved and not saved:continue
      body=fn
      for base in (0,2):
        old=f'\ty0 = d->acc_b[{base}];\n\tx0 = d->acc_a[{base}];\n\td->acc_b[{base}] = (short)acc;\n\td->acc_a[{base}] = (short)'+('(in >> 4)' if base==0 else 'stage1')+f';\n\td->acc_b[{base+1}] = (short)y0;\n\td->acc_a[{base+1}] = (short)x0;'
        lines=old.splitlines();order=[0,2,1,3,4,5] if interleaved else list(range(6))
        if saved:order[-2:]=[5,4]
        assert body.count(old)==1
        body=body.replace(old,'\n'.join(lines[i] for i in order))
      cells[f'biquad-interleave-{int(interleaved)}-saved-{int(saved)}']=source[:start]+body+source[end:]
    start,end,fn=driver.function(source,'v8_detectorinit')
    old='''\tfor (i = 0; i < 4; i++) {
\t\td->acc_a[i] = 0;
\t\td->acc_b[i] = 0;
\t}'''
    new='''\tfor (i = 0; i < 2; i++) {
\t\tfor (j = 0; j < 2; j++) {
\t\t\td->acc_a[2 * i + j] = 0;
\t\t\td->acc_b[2 * i + j] = 0;
\t\t}
\t}'''
    for width,empty,order in itertools.product(('int','short'),(False,True),(False,True)):
      body=fn.replace('\tint i;',f'\t{width} i, j;').replace(old,new)
      if empty:body=body.replace('\tfor (i = 0; i < 3; i++) {','\tfor (i = 0; i < 3; i++) {\n\t\tfor (j = 0; j < 3; j++)\n\t\t\t;')
      if order:body=body.replace('\td->table = table;\n\td->armed = 0;','\td->armed = 0;\n\td->table = table;')
      cells[f'detector-{width}-empty-{int(empty)}-order-{int(order)}']=source[:start]+body+source[end:]
    cells['combined-exact'] = cells['detector-short-empty-1-order-0'].replace('i < 64', 'i < pr->half * 2')
    assert len(cells)==len(set(cells.values()))==16
    return cells

if __name__=='__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
    driver.REV='80c5dea3';driver.OUT_NAME='gcc3-batch5-v8-detector';driver.SOURCE_PATHS=('src/v8/V8Detector.c',);driver.variants=variants;driver.main()
