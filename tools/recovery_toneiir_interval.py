#!/usr/bin/env python3
"""Declared eight-cell interval-storage x SAR x expanded-short seed cross."""
import sys, subprocess
from pathlib import Path
import playbook_small_patterns as d
from gcc3_batch20_toneiir_sections_reproduce import expanded
HEADER = subprocess.check_output(['git','show','426814ea:include/dsplib/toneiir.h'],cwd=d.ROOT,text=True)
def wide(label):
    return 'wide-1' in label
def overlays(path,label):
    if not wide(label): return {}
    old='short\t\tinterval;\t/* +0x10 */\n\tshort\t\tpad12;\t\t/* +0x12 */'
    assert HEADER.count(old)==1
    return {'dsplib/toneiir.h':HEADER.replace(old,'int\t\tinterval;\t/* +0x10 */')}
def variants(path,source):
    if path.endswith('Cadence.c'):
        assert source.count('cfg.interval = (short)c->buflen;') == 1
        return {'baseline':source, 'producer-wide-1':source.replace('cfg.interval = (short)c->buflen;', 'cfg.interval = c->buflen;')}
    cells={}
    for seed in (0,1):
      for width in (0,1):
       for sar in (0,1):
        label='baseline' if not(seed or width or sar) else f'seed-{seed}-wide-{width}-sar-{sar}'
        text=source
        if seed:
          for name in ('toneiir_progress','_iir_filter_progress'):
            a,z,fn=d.function(text,name)
            assert fn.count('int k;')==1
            fn=expanded(fn.replace('int k;','short k;'))
            text=text[:a]+fn+text[z:]
        if width:
          assert text.count('\t.pad12\t\t= 0,\n')==1
          text=text.replace('\t.pad12\t\t= 0,\n','')
        if sar:
          assert text.count('st->env_in / 4')==1
          text=text.replace('st->env_in / 4','(st->env_in >> 2)')
        cells[label]=text
    assert len(set(cells.values()))==8
    return cells
if __name__=='__main__':
 d.REV='426814ea';d.OUT_NAME='recovery-toneiir-interval';d.SOURCE_PATHS=('src/callprog/toneiir.c','src/callprog/Cadence.c');d.variants=variants;d.HEADER_OVERLAYS=overlays;d.main()
