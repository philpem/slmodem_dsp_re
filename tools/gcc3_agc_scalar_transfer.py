#!/usr/bin/env python3
"""Cross AGC scalar producer mode and the original retained floor capture."""
import subprocess
import playbook_small_patterns as d


def variants(path, source):
    return {label:source for label in ('baseline','producer','floor','both')}


def overlays(path, label):
    if label == 'baseline':
        return {}
    text = subprocess.check_output(['git','show','3986addf:include/dsplib/Agc.h'],cwd=d.ROOT,text=True)
    if label in ('producer','both'):
        old = 'double lvl = (1.0 / blockLen) * acc;'
        assert text.count(old) == 1
        text = text.replace(old,'T lvl = (T(1) / blockLen) * acc;')
        old = 'double t =\n\t\t\t\t\t    agc_fsqrt((1.0 / lvl) * ref);'
        assert text.count(old) == 1
        text = text.replace(old,'T t =\n\t\t\t\t\t    agc_fsqrt((T(1) / lvl) * ref);')
        assert text.count('+ (1.0 - alpha) * t') == 1
        text = text.replace('+ (1.0 - alpha) * t','+ (T(1) - alpha) * t')
    if label in ('floor','both'):
        assert text.count('long double minLevel = 1e-10f;') == 1
        text = text.replace('long double minLevel = 1e-10f;','T minLevel = 1e-10f;')
    return {'dsplib/Agc.h':text}


if __name__ == '__main__':
    d.REV = '3986addf'
    d.SOURCE_PATHS = ('src/pump/v90/V90Demodulator.cpp',)
    d.OUT_NAME = 'gcc3-agc-scalar-transfer'
    d.DUMP_FLAGS = ('-v','-save-temps','-dr','-dc','-dg')
    d.variants = variants
    d.HEADER_OVERLAYS = overlays
    d.main()
