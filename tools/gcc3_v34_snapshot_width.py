#!/usr/bin/env python3
"""Cross two independently witnessed 16-bit snapshot locals in the full TU."""
import sys
from pathlib import Path
import playbook_small_patterns as d

HEADER = 'dsplib/v34hstx1_arms.h'


def variants(path, source):
    return dict.fromkeys(('baseline', 'index-word', 'difference-word', 'both-word'), source)


def overlays(path, label):
    text = (d.ROOT / 'include' / HEADER).read_text()
    old = '\tint sum = 0, i, t;'
    assert text.count(old) == 1
    if label == 'index-word':
        text = text.replace(old, '\tint sum = 0, t;\n\tshort i;')
    elif label == 'difference-word':
        text = text.replace(old, '\tint sum = 0, i;\n\tshort t;')
    elif label == 'both-word':
        text = text.replace(old, '\tint sum = 0;\n\tshort i, t;')
    else:
        assert label == 'baseline'
        return {}
    return {HEADER: text}


if __name__ == '__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain') + 1]).is_file()
    d.REV = '7edfb734'
    d.OUT_NAME = 'gcc3-v34-snapshot-width'
    d.SOURCE_PATHS = ('src/pump/v34/V34hshak.c',)
    d.variants = variants
    d.HEADER_OVERLAYS = overlays
    d.main()
