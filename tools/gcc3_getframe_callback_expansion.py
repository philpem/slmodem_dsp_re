#!/usr/bin/env python3
"""Replay original fixed getFrame groups, captured callback and word locals."""
import itertools
import sys
from pathlib import Path
import playbook_small_patterns as d


def variants(path, source):
    a, z, fn = d.function(source, 'getFrame')
    marker = '\tfor (g = 0; g < 4; g++) {'
    begin = fn.index(marker)
    body = fn[begin+len(marker):fn.rfind('\n\t}')]
    assert body.count('g == 3') == body.count('g * 4') == 1
    groups = []
    for g in range(4):
        group = body.replace('&s->frame[2 + g * 4]', '&s->frame[%d]' % (2+g*4))
        group = group.replace('(g == 3) ? small_last : small', 'small_last' if g == 3 else 'small')
        groups.append('\t{' + group + '\n\t}')
    expanded = fn[:begin] + '\n\n'.join(groups) + '\n}'
    cells = {'baseline': source}
    for expand, capture, word in itertools.product((False, True), repeat=3):
        if not (expand or capture or word):
            continue
        text = expanded if expand else fn
        if expand:
            text = text.replace('\tint g;\n', '')
        if capture:
            text = text.replace('\tint w = s->idx_width;', '\tv34_scramble_fn scramble = s->scramble;\n\tint w = s->idx_width;')
            assert text.count('s->scramble(') == (6 if expand else 3)
            text = text.replace('s->scramble(', 'scramble(')
        if word:
            for name in ('nb', 'pos'):
                text = text.replace('\tint '+name+';', '\tshort '+name+';')
            for name in ('small', 'small_last'):
                text = text.replace('\tint '+name+' = 2;', '\tshort '+name+' = 2;')
        cells['expand-%d-capture-%d-word-%d' % (expand,capture,word)] = source[:a] + text + source[z:]
    assert len(cells) == len(set(cells.values())) == 8
    return cells


if __name__ == '__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
    d.REV = '80da7b0c'
    d.OUT_NAME = 'gcc3-getframe-callback-expansion'
    d.SOURCE_PATHS = ('src/pump/v34/v34shell.c',)
    d.variants = variants
    d.main()
