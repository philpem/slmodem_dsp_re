#!/usr/bin/env python3
"""Correct combined-loop coefficient read age before the history transfer."""
import sys
from pathlib import Path
import playbook_small_patterns as d
from gcc3_fskdetect_phase_delay import variants as prior_variants


def variants(path, source):
    prior = prior_variants(path, source)
    cells = {'baseline': source}
    for label, text in prior.items():
        if not label.startswith('transfer-1-'):
            continue
        old = '\t\t\t\tshort sample = hist[V34_FSK_TAPS - 1 - k];'
        assert text.count(old) == 1 and text.count('intcoef3[k] * sample') == 1
        text = text.replace(old, old+'\n\t\t\t\tshort coefficient = intcoef3[k];')
        text = text.replace('intcoef3[k] * sample', 'coefficient * sample')
        cells[label+'-coefficient-before-store'] = text
    assert len(cells) == len(set(cells.values())) == 5
    return cells


if __name__ == '__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
    d.REV = '80da7b0c'
    d.OUT_NAME = 'gcc3-fskdetect-phase-read-age'
    d.SOURCE_PATHS = ('src/pump/v34/DPSK.c',)
    d.variants = variants
    d.main()
