#!/usr/bin/env python3
"""Cross the recorded decision source cells with bounded signed result APIs."""
import subprocess
import sys
import playbook_small_patterns as driver
import playbook_v34_decision as source_cells

REV = '361ef919'
HEADER = 'include/dsplib/v34rx.h'


def variants(path, source):
    cells = {}
    for label, text in source_cells.variants(path, source).items():
        cells[label] = text
        start, end, fn = driver.function(text, 'decision')
        assert text[start - 5:start] == 'void\n'
        marker = '\td->dp.point = *best;\n'
        assert fn.count(marker) == 1
        fn = fn.replace(marker, marker + '\treturn (short)(best - pts);\n')
        for result in ('short', 'int'):
            cells[label + '_return-' + result] = text[:start - 5] + result + '\n' + fn + text[end:]
    assert len(set(cells.values())) == 12
    return cells


def overlays(path, label):
    if '_return-' not in label:
        return {}
    result = label.rsplit('_return-', 1)[1]
    header = subprocess.check_output(
        ['git', 'show', REV + ':' + HEADER], cwd=driver.ROOT, text=True)
    assert header.count('void decision(') == 1
    return {'dsplib/v34rx.h': header.replace('void decision(', result + ' decision(')}


if __name__ == '__main__':
    assert '--domain' in sys.argv
    assert sys.argv[sys.argv.index('--domain') + 1].startswith(
        'https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-')
    driver.REV = REV
    driver.OUT_NAME = 'playbook-v34-decision-return'
    driver.SOURCE_PATHS = ('src/pump/v34/V34RX.c',)
    driver.variants = variants
    driver.HEADER_OVERLAYS = overlays
    driver.main()
