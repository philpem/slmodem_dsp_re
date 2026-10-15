#!/usr/bin/env python3
"""Discriminate the original searched-index success/default publication graph."""
import sys
from pathlib import Path
import playbook_small_patterns as d


def variants(path, source):
    a, z, fn = d.function(source, 'dftenergy')
    assert fn.count('\t\t\t\tbreak;') == 1
    jump = fn.replace('\t\t\t\tbreak;', '\t\t\t\tgoto next_bin;')
    jump = jump.replace('\t\tif (k > 0x12)\n\t\t\tbins->shift = 0x14;',
                        '\t\tbins->shift = 0x14;\nnext_bin:\n\t\t;')
    single = fn.replace('\t\t\t\tbins->shift = (short)(k + 1);\n', '')
    single = single.replace('\t\tif (k > 0x12)\n\t\t\tbins->shift = 0x14;',
                            '\t\tbins->shift = (short)(k + 1);')
    cells = {'baseline': source, 'success-jump': source[:a]+jump+source[z:],
             'single-publication': source[:a]+single+source[z:]}
    assert len(set(cells.values())) == 3
    return cells


if __name__ == '__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
    d.REV = '80da7b0c'
    d.OUT_NAME = 'gcc3-dftenergy-loop-exit'
    d.SOURCE_PATHS = ('src/pump/v34/DFTC.c',)
    d.variants = variants
    d.main()
