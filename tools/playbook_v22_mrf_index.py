#!/usr/bin/env python3
"""Four complete-TU work-index lifetime/sequencing controls for V22 MRF init."""
from itertools import product
import playbook_small_patterns as driver


def variants(path, source):
    start, end, body = driver.function(source, 'V22_MRF_init')
    cells = {}
    for early, separate in product((False, True), repeat=2):
        text = body
        if early:
            assert text.count('\tshort i, j, k;') == text.count('\tk = 0;') == 1
            text = text.replace('\tshort i, j, k;', '\tshort i, j, k = 0;').replace('\tk = 0;\n', '')
        if separate:
            old = '\t\tfor (i = j; i >= 0; i -= V22_MRF_PHASES)\n\t\t\twork[k++] = coeff[i];'
            new = '\t\tfor (i = j; i >= 0; i -= V22_MRF_PHASES) {\n\t\t\twork[k] = coeff[i];\n\t\t\tk++;\n\t\t}'
            assert text.count(old) == 1
            text = text.replace(old, new)
        label = 'baseline' if not (early or separate) else '-'.join(n for n, yes in (('early',early),('separate',separate)) if yes)
        cells[label] = source[:start] + text + source[end:]
    assert len(cells) == len(set(cells.values())) == 4
    return cells


if __name__ == '__main__':
    driver.REV = 'b8081307'
    driver.OUT_NAME = 'playbook-v22-mrf-index'
    driver.SOURCE_PATHS = ('src/pump/v22/v22_mrf.c',)
    driver.variants = variants
    driver.main()
