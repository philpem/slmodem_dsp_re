#!/usr/bin/env python3
"""Replay the declared eight-cell V34 DFT input/arithmetic scope cross."""
import itertools
import argparse
import sys
from pathlib import Path
import playbook_small_patterns as driver


def variants(path, source):
    start,end,fn=driver.function(source,'dftupdate')
    outer='\tfor (i = 0; i < nsamples; i++) {'
    load='\t\tint x = samples[i];\n'
    idx='\t\t\tidx = (unsigned)b->phase >> V34_DFT_PHASE_SHIFT;'
    im='\t\t\tim = costbl[(idx + V34_DFT_QUARTER) & 0xff] * x;\n'
    update='\t\t\tb->acc_im = (int)((unsigned)b->acc_im'
    assert all(fn.count(v)==1 for v in (outer,load,idx,im,update))
    forms={}
    for cursor,inside,interleave in itertools.product((False,True),repeat=3):
        text=fn
        if cursor:
            text=text.replace(outer,'\tfor (i = 0; i < nsamples; i++, samples++) {').replace('samples[i]','*samples')
        if inside:
            text=text.replace('\t\tint x = '+('*samples' if cursor else 'samples[i]')+';\n','')
            text=text.replace('\t\t\tint re, im;','\t\t\tint re, im;\n\t\t\tint x;')
            text=text.replace(idx,idx+'\n\t\t\tx = '+('*samples' if cursor else 'samples[i]')+';')
        if interleave:
            text=text.replace(im,'').replace(update,im+update)
        label='-'.join(n for n,v in [('cursor',cursor),('per-bin',inside),('interleaved',interleave)] if v) or 'baseline'
        forms[label]=source[:start]+text+source[end:]
    if CHANNEL_ORDER:
        combined=forms['cursor-per-bin-interleaved']
        channel_only=forms['interleaved']
        line='\t\t\tb->sum_re += (double)re;\n'
        def move_channel(text):
            assert text.count(line)==1 and text.count(im)==1
            return text.replace(line,'').replace(im,line+im)
        forms={'baseline':source, 'channel-only':move_channel(channel_only), 'cursor-per-bin-interleaved':combined, 'cursor-per-bin-channel':move_channel(combined)}
    assert len(forms)==len(set(forms.values()))==(4 if CHANNEL_ORDER else 8)
    return forms


if __name__=='__main__':
    parser=argparse.ArgumentParser(add_help=False)
    parser.add_argument('--channel-order',action='store_true')
    opts,remaining=parser.parse_known_args()
    CHANNEL_ORDER=opts.channel_order
    sys.argv=sys.argv[:1]+remaining
    assert '--domain' in sys.argv
    assert Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
    driver.REV='6981ee37'
    driver.OUT_NAME='gcc3-v34-dft-boundaries'+('-channel' if CHANNEL_ORDER else '')
    driver.SOURCE_PATHS=('src/pump/v34/DFTC.c',)
    driver.variants=variants
    driver.main()
