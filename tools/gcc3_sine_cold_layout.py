#!/usr/bin/env python3
"""Trace/test SineWave's independently observed first-iteration ownership."""
import subprocess
import playbook_small_patterns as d


def variants(path, source):
    return {'baseline':source,'first-iteration-owner':source}


def overlays(path, label):
    if label == 'baseline':
        return {}
    text = subprocess.check_output(['git','show','2180f9d0:include/dsplib/SineWave.h'],cwd=d.ROOT,text=True)
    start = text.index('\tunsigned long i = 0;',text.index('void SineWave<Tout, Tparam>::generate('))
    end = text.index('\n\t/*\n\t * The wrap:',start)
    replacement = '''\tunsigned long i;
\tfor (i = 0; i < n; i++) {
\t\tTparam s;
\t\tif (i == 0)
\t\t\tx = phase;
\t\telse
\t\t\tphase = x;
\t\ts = sinl(x);
\t\tout[i] = (Tout)(s * amplitude);
\t\tx = phase + step;
\t}
\tif (i == 0)
\t\tx = phase;
'''
    return {'dsplib/SineWave.h':text[:start]+replacement+text[end:]}


if __name__ == '__main__':
    d.REV = '2180f9d0'
    d.SOURCE_PATHS = ('src/pump/v90/VpcmFloModem.cpp',)
    d.OUT_NAME = 'gcc3-sine-cold-layout'
    d.DUMP_FLAGS = ('-v','-save-temps','-da')
    d.variants = variants
    d.HEADER_OVERLAYS = overlays
    d.main()
