#!/usr/bin/env python3
"""Eight crossed return-slot/count/branch controls after the B103 witness cross."""
from itertools import product
import subprocess
import playbook_small_patterns as driver
import recovery_b103_return_boundaries as first
from recovery_sre_boundary import REV
HEADER = 'include/dsplib/b103fp.h'

def variants(path, source):
    base = first.variants(path, source)['u1_i1_o1_a1']
    cells = {'baseline': source}
    for full, count, success in product((0,1), repeat=3):
        text = base
        if full:
            old = 'short\nDemodDataB103('
            assert text.count(old) == 1
            text = text.replace(old, 'int\nDemodDataB103(')
            start,end,body=driver.function(text,'DemodDataB103')
            assert body.count('return (short)nbits;') == 1
            text=text[:start]+body.replace('return (short)nbits;', 'return nbits;')+text[end:]
        if count:
            start,end,body=driver.function(text,'DemodDataB103')
            assert body.count('i < (short)count') == 2
            text=text[:start]+body.replace('i < (short)count','i < count')+text[end:]
        if success:
            old = """if (FPM_TONE_detect(fp->hdx->tone_detect, fp->dsp->rx_scratch,
\t\t\t\t    (short)count) != FPM_TONE_PRESENT) {
\t\t\tfp->dsp->rx_state = 0;
\t\t} else {
\t\t\tfp->dsp->rx_tone = 1;
\t\t\tfp->dsp->rx_state = (short)(fp->dsp->rx_state + 5);
\t\t}"""
            new = """if (FPM_TONE_detect(fp->hdx->tone_detect, fp->dsp->rx_scratch,
\t\t\t\t    (short)count) == FPM_TONE_PRESENT) {
\t\t\tfp->dsp->rx_tone = 1;
\t\t\tfp->dsp->rx_state = (short)(fp->dsp->rx_state + 5);
\t\t} else {
\t\t\tfp->dsp->rx_state = 0;
\t\t}"""
            assert text.count(old) == 1
            text=text.replace(old,new)
        cells['r%d_c%d_s%d'%(full,count,success)] = text
    assert len(cells) == len(set(cells.values())) == 9
    return cells

def overlays(path,label):
    if not label.startswith('r1_'): return {}
    text=subprocess.check_output(['git','show',REV+':'+HEADER],cwd=driver.ROOT,text=True)
    old='short DemodDataB103('
    assert text.count(old)==1
    return {'dsplib/b103fp.h':text.replace(old,'int DemodDataB103(')}

if __name__ == '__main__':
    driver.REV=REV
    driver.OUT_NAME='recovery-b103-return-slots'
    driver.SOURCE_PATHS=('src/pump/b103/B103prc.c',)
    driver.variants=variants
    driver.HEADER_OVERLAYS=overlays
    driver.DUMP_FLAGS=()
    driver.main()
