#!/usr/bin/env python3
"""Test the original phase read boundary after scalar expansion diagnosis."""
import playbook_small_patterns as d
from gcc3_sine_scalar_transfer import overlays as scalar_overlay


def variants(path, source):
    return {'baseline': source, 'phase-after-guard': source, 'phase-after-guard-less': source}


def overlays(path, label):
    text = scalar_overlay(path, 'scalar-sinl-entry')['dsplib/SineWave.h']
    if label.startswith('phase-after-guard'):
        start = text.index('\tunsigned long i = 0;',text.index('void SineWave<Tout, Tparam>::generate('))
        end = text.index('\n\t/*\n\t * The wrap:',start)
        replacement = '''\tunsigned long i = 0;
\tif (n > i) {
\t\tx = phase;
\t\tdo {
\t\t\tTparam s = sinl(x);
\t\t\tout[i] = (Tout)(s * amplitude);
\t\t\t++i;
\t\t\tx = phase + step;
\t\t\tif (n > i)
\t\t\t\tphase = (Tparam)x;
\t\t} while (n > i);
\t} else {
\t\tx = phase;
\t}
'''
        if label.endswith('-less'):
            replacement = replacement.replace('n > i', 'i < n')
        text = text[:start]+replacement+text[end:]
    return {'dsplib/SineWave.h':text}


if __name__ == '__main__':
    d.REV = '3986addf'
    d.SOURCE_PATHS = ('src/pump/v90/VpcmFloModem.cpp',)
    d.OUT_NAME = 'gcc3-sine-phase-boundary'
    d.DUMP_FLAGS = ('-v','-save-temps','-da')
    d.variants = variants
    d.HEADER_OVERLAYS = overlays
    d.main()
