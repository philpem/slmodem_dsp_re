#!/usr/bin/env python3
"""Cross searched-index exits with original low-byte shift-count consumers."""
import sys
from pathlib import Path
import playbook_small_patterns as d
from gcc3_dftenergy_loop_exit import variants as exit_variants


def variants(path, source):
    cells = {}
    for exit_name, text in exit_variants(path, source).items():
        cells[exit_name] = text
        a, z, fn = d.function(text, 'dftenergy')
        old = '\t\tunsigned sh = (unsigned char)scale & 31;'
        assert fn.count(old) == 1 and fn.count(' << sh') == 2
        byte = fn.replace(old, '\t\tunsigned char sh = (unsigned char)scale & 31;')
        direct = fn.replace(old + '\n', '').replace(' << sh', ' << ((unsigned char)scale & 31)')
        cells[exit_name+'-byte-count'] = text[:a]+byte+text[z:]
        cells[exit_name+'-direct-count'] = text[:a]+direct+text[z:]
    assert len(cells) == len(set(cells.values())) == 9
    return cells


if __name__ == '__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
    d.REV = '80da7b0c'
    d.OUT_NAME = 'gcc3-dftenergy-count-exit'
    d.SOURCE_PATHS = ('src/pump/v34/DFTC.c',)
    d.variants = variants
    d.main()
