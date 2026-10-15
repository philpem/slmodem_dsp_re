#!/usr/bin/env python3
"""Cross witnessed SineWave expansion mode with its counted-entry graph."""
import playbook_small_patterns as d
import subprocess


def variants(path, source):
    return {label: source for label in ('baseline','entry','scalar-sinl',
            'scalar-sinf','scalar-sinl-entry','scalar-sinf-entry')}


def overlays(path, label):
    if label == 'baseline':
        return {}
    text = subprocess.check_output(['git','show','3986addf:include/dsplib/SineWave.h'],cwd=d.ROOT,text=True)
    if label.endswith('entry'):
        start = text.index('\tif (n == 0) {',text.index('void SineWave<Tout, Tparam>::generate('))
        end = text.index('\n\t/*\n\t * The wrap:',start)
        replacement = '''\tunsigned long i = 0;
\tx = phase;
\twhile (i < n) {
\t\tlong double s = sinl(x);
\t\tout[i] = (Tout)(s * (long double)amplitude);
\t\t++i;
\t\tx = (long double)phase + step;
\t\tif (i < n)
\t\t\tphase = (Tparam)x;
\t}
'''
        text = text[:start]+replacement+text[end:]
    if label.startswith('scalar'):
        start = text.index('void SineWave<Tout, Tparam>::generate(')
        head, body = text[:start], text[start:]
        body = body.replace('long double', 'Tparam')
        body = body.replace('(Tparam)frequency', 'frequency').replace('(Tparam)amplitude', 'amplitude')
        body = body.replace('(Tparam)phase', 'phase').replace('(Tparam)t', 't')
        if label.startswith('scalar-sinf'):
            assert body.count('sinl(x)') == 1
            body = body.replace('sinl(x)', 'sinf(x)')
        text = head + body
    return {'dsplib/SineWave.h':text}


if __name__ == '__main__':
    d.REV = '3986addf'
    d.SOURCE_PATHS = ('src/pump/v90/VpcmFloModem.cpp',)
    d.OUT_NAME = 'gcc3-sine-scalar-transfer'
    d.DUMP_FLAGS = ('-v','-save-temps','-da')
    d.variants = variants
    d.HEADER_OVERLAYS = overlays
    d.main()
