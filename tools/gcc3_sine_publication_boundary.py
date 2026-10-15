#!/usr/bin/env python3
"""Test SineWave's visible phase publication before sine/output consumption."""
import playbook_small_patterns as d
from gcc3_sine_phase_boundary import overlays as owner_overlay


def variants(path, source):
    return {'baseline':source, 'phase-before-output':source}


def overlays(path, label):
    text = owner_overlay(path, 'phase-after-guard-less')['dsplib/SineWave.h']
    if label == 'phase-before-output':
        start = text.index('\tunsigned long i = 0;',text.index('void SineWave<Tout, Tparam>::generate('))
        end = text.index('\n\t/*\n\t * The wrap:',start)
        replacement = '''\tunsigned long i;
\tx = phase;
\tfor (i = 0; i < n; i++) {
\t\tTparam s;
\t\tphase = x;
\t\ts = sinl(x);
\t\tout[i] = (Tout)(s * amplitude);
\t\tx = phase + step;
\t}
'''
        text = text[:start]+replacement+text[end:]
    return {'dsplib/SineWave.h':text}


if __name__ == '__main__':
    d.REV = '3986addf'
    d.SOURCE_PATHS = ('src/pump/v90/VpcmFloModem.cpp',)
    d.OUT_NAME = 'gcc3-sine-publication-boundary'
    d.DUMP_FLAGS = ('-v','-save-temps','-da')
    d.variants = variants
    d.HEADER_OVERLAYS = overlays
    d.main()
