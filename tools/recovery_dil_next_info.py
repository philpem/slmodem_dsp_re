#!/usr/bin/env python3
"""Cross the witnessed shared V92 info base and final parity CFG."""
from itertools import product
import playbook_small_patterns as d

REV = 'd9995470'
PATHS = ('src/pump/v90/V90DILdesPCK.cpp', 'src/pump/v90/V92DILdesPCK.cpp')


def variants(path, source):
    if 'V90DIL' in path:
        return {'baseline': source}
    cells = {}
    for capture, parity in product((0, 1), repeat=2):
        text = source
        if capture:
            old = '''\tfor (i = 0; i < 16; i++)
\t\tbits[infoAt + 1 + i] = v92Info[i];
\tbits[infoAt + 17] = 0;
\tfor (i = 16; i < 19; i++)
\t\tbits[infoAt + 2 + i] = v92Info[i];
\tfor (i = 20; i < 33; i++)
\t\tbits[infoAt + 1 + i] = 0;'''
            new = '''\t{
\t\tunsigned char *info = bits + infoAt + 1;

\t\tfor (i = 0; i < 16; i++)
\t\t\tinfo[i] = v92Info[i];
\t\tinfo[16] = 0;
\t\tfor (i = 16; i < 19; i++)
\t\t\tinfo[1 + i] = v92Info[i];
\t\tfor (i = 20; i < 33; i++)
\t\t\tinfo[i] = 0;
\t}'''
            assert text.count(old) == 1
            text = text.replace(old, new)
        if parity:
            old = '''\tif (crcAt & 1) {
\t\t*nbits = crcAt + 19;
\t\tbits[crcAt + 18] = 0;
\t} else {
\t\t*nbits = crcAt + 18;
\t}'''
            new = '''\tif (!(crcAt & 1)) {
\t\t*nbits = crcAt + 18;
\t} else {
\t\t*nbits = crcAt + 19;
\t\tbits[crcAt + 18] = 0;
\t}'''
            assert text.count(old) == 1
            text = text.replace(old, new)
        label = 'baseline' if not (capture or parity) else 'base-%d-even-%d' % (capture, parity)
        cells[label] = text
    assert len(cells) == len(set(cells.values())) == 4
    return cells


if __name__ == '__main__':
    d.REV = REV
    d.OUT_NAME = 'recovery-dil-next-info'
    d.SOURCE_PATHS = PATHS
    d.variants = variants
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da')
    d.main()
