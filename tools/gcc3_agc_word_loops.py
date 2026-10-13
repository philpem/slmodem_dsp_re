#!/usr/bin/env python3
"""Cross original AGC word counters, gate operands and pointer countdowns."""
import sys
import playbook_small_patterns as d
from gcc3_v23_agc_return import variants as return_variants, overlays


def variants(path,source):
    fixed=return_variants(path,source)['return-only']
    cells={}
    for bits in range(8):
        text=fixed
        label='baseline' if bits==0 else 'word-'+str(bits)
        if bits&1:
            assert text.count('\tint i;')==1
            text=text.replace('\tint i;','\tshort i;')
        if bits&2:
            for old,new in [('const int acquire_level','const short acquire_level'),('const int squelch_level','const short squelch_level'),('int level = FPM_rms','short level = FPM_rms')]:
                assert text.count(old)==1
                text=text.replace(old,new)
        if bits&4:
            assert text.count('\t\tint k;')==1
            text=text.replace('\t\tint k;','\t\tunsigned short k;\n\t\tshort *p;')
            old='\t\t\tfor (k = 0; k < (int)len; k++)\n\t\t\t\tsamples[k] = 0;'
            assert text.count(old)==1
            text=text.replace(old,'\t\t\tp = samples;\n\t\t\tk = len;\n\t\t\twhile (k--)\n\t\t\t\t*p++ = 0;')
            old='\t\tfor (k = 0; k < (int)len; k++) {\n\t\t\tint v = (short)((samples[k] * mult) >> 15);'
            assert text.count(old)==1
            text=text.replace(old,'\t\tp = samples;\n\t\tk = len;\n\t\twhile (k--) {\n\t\t\tint v = (short)((*p * mult) >> 15);')
            for old,new in [('samples[k] = 32767','*p = 32767'),('samples[k] = -32768','*p = -32768'),('samples[k] = (short)v;','*p = (short)v;\n\t\t\tp++;')]:
                assert text.count(old)==1
                text=text.replace(old,new)
        cells[label]=text
    assert len(set(cells.values()))==8
    return cells


def header(path,label):
    return overlays(path,'return-only')


def gain_variants(path,source):
    prior=variants(path,source)
    cells={}
    for bits in range(8):
        text=prior['word-7' if bits&1 else 'word-3']
        if bits&2:
            assert text.count('int mult = agc->mult;')==1
            text=text.replace('int mult = agc->mult;','short mult = agc->mult;')
        if bits&4:
            assert text.count('int level_est = agc->level;')==1
            text=text.replace('int level_est = agc->level;','short level_est = agc->level;')
        cells['baseline' if bits==0 else 'gain-'+str(bits)]=text
    assert len(set(cells.values()))==8
    return cells


if __name__=='__main__':
    d.REV='ad83fa7c'
    d.SOURCE_PATHS=('src/dsp/fpm_agc.c',)
    gain='--gain' in sys.argv
    if gain:
        sys.argv.remove('--gain')
    d.OUT_NAME='gcc3-agc-gain-words' if gain else 'gcc3-agc-word-loops'
    d.DUMP_FLAGS=('-v','-save-temps','-da')
    d.variants=gain_variants if gain else variants
    d.HEADER_OVERLAYS=header
    d.main()
