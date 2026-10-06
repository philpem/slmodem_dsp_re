#!/usr/bin/env python3
"""Cross original length-publication arms and reset warmup countdown."""
import playbook_small_patterns as d


def variants(path, source):
    cap = '''\tif (length < 8160u)
\t\tlength = 8160u;
\ttrn1uLength = length;
\tedprintf("V92Phase3Modulator: TRN1u state length set to %d\\r\\n",
\t    trn1uLength);'''
    arms = '''\tif (length > 8160u) {
\t\ttrn1uLength = length;
\t\tedprintf("V92Phase3Modulator: TRN1u state length set to %d\\r\\n",
\t\t    trn1uLength);
\t} else {
\t\ttrn1uLength = 8160u;
\t\tedprintf("V92Phase3Modulator: TRN1u state length set to %d\\r\\n",
\t\t    trn1uLength);
\t}'''
    loop = '\tfor (i = 0; i < nSymbols; i++)\n\t\tgenerateSymbol();'
    assert source.count(cap) == source.count(loop) == 1
    cells = {}
    for split in (False, True):
        for countdown in (False, True):
            text = source
            if split:
                text = text.replace(cap, arms)
            if countdown:
                text = text.replace(loop, '\tfor (i = nSymbols; i != 0; i--)\n\t\tgenerateSymbol();')
            cells['baseline' if not (split or countdown) else 'split-%d-countdown-%d' % (split, countdown)] = text
    return cells


if __name__ == '__main__':
    d.REV = '276d30b5'
    d.SOURCE_PATHS = ('src/pump/v90/V92Phase3Modulator.cpp',)
    d.OUT_NAME = 'v92-reset-branch'
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dP')
    d.variants = variants
    d.main()
