#!/usr/bin/env python3
"""Replay original fixed-group callback and short-width witnesses in V34 shell."""
import itertools
import sys
from pathlib import Path
import playbook_small_patterns as d


def variants(path, source):
    start, end, fn = d.function(source, 'putFrame')
    old = '''\tfor (g = 0; g < 4; g++) {
\t\tconst short *p = &v[2 + g * 4];

\t\tput(s, (unsigned short)p[0], 1);
\t\tput(s, (unsigned short)p[1], g == 3 ? small_last : small);
\t\tput(s, (unsigned short)p[2], w);
\t\tput(s, (unsigned short)p[3], w);
\t}'''
    assert fn.count(old) == 1
    expanded = '\n\n'.join('\n'.join(
        '\tput(s, (unsigned short)v[%d], %s);' % (2 + group * 4 + lane, width)
        for lane, width in enumerate(('1', 'small_last' if group == 3 else 'small', 'w', 'w')))
        for group in range(4))
    cells = {'baseline': source}
    for expansion, nbword, smallword in itertools.product((False, True), repeat=3):
        if not (expansion or nbword or smallword):
            continue
        text = fn
        if expansion:
            text = text.replace('\tint g;\n', '').replace(old, expanded)
        if nbword:
            text = text.replace('\tint nb;', '\tshort nb;')
        if smallword:
            text = text.replace('\tint small = 2;', '\tshort small = 2;')
            text = text.replace('\tint small_last = 2;', '\tshort small_last = 2;')
        label = 'expand-%d-nbword-%d-smallword-%d' % (expansion, nbword, smallword)
        cells[label] = source[:start] + text + source[end:]
    assert len(cells) == len(set(cells.values())) == 8
    return cells


if __name__ == '__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain') + 1]).is_file()
    d.REV = '7edfb734'
    d.OUT_NAME = 'gcc3-v34-putframe-expansion'
    d.SOURCE_PATHS = ('src/pump/v34/v34shell.c',)
    d.variants = variants
    d.main()
