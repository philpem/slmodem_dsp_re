#!/usr/bin/env python3
"""Bounded original path and initialized-width lifetime cross after expansion."""
import itertools
import sys
from pathlib import Path
import playbook_small_patterns as d
from gcc3_v34_putframe_expansion import variants as expansion_variants


def variants(path, source):
    expanded = expansion_variants(path, source)['expand-1-nbword-1-smallword-0']
    start, end, fn = d.function(expanded, 'putFrame')
    old = '''\tif ((unsigned short)s->span > (unsigned short)sum) {
\t\ts->wide_accum = (short)sum;
\t\tnb = s->wide_bits_alt;
\t} else {
\t\ts->wide_accum = (short)(sum - s->span);
\t\tnb = s->wide_bits;
\t}'''
    inverse = '''\tif ((unsigned short)sum >= (unsigned short)s->span) {
\t\ts->wide_accum = (short)(sum - s->span);
\t\tnb = s->wide_bits;
\t} else {
\t\ts->wide_accum = (short)sum;
\t\tnb = s->wide_bits_alt;
\t}'''
    defaults = '\tint small = 2;\n\tint small_last = 2;\n'
    assert fn.count(old) == fn.count(defaults) == 1
    cells = {'baseline': source}
    for mainfull, early in itertools.product((False, True), repeat=2):
        text = fn
        if mainfull:
            text = text.replace(old, inverse)
        if early:
            text = text.replace(defaults, '')
            text = text.replace('{\n', '{\n' + defaults, 1)
        cells['full-%d-early-%d' % (mainfull, early)] = expanded[:start] + text + expanded[end:]
    assert len(cells) == len(set(cells.values())) == 5
    return cells


if __name__ == '__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain') + 1]).is_file()
    d.REV = '7edfb734'
    d.OUT_NAME = 'gcc3-v34-putframe-capture'
    d.SOURCE_PATHS = ('src/pump/v34/v34shell.c',)
    d.variants = variants
    d.main()
