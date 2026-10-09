#!/usr/bin/env python3
"""Cross original permutation-counter lifetime and consumed-index order."""
import sys
from pathlib import Path
import playbook_small_patterns as d

def variants(path, source):
    start, end, original = d.function(source, 'V22_MRF_init')
    cells = {'baseline': source}
    for early, separate, preload in ((True, False, False), (False, True, False),
                                    (True, True, False), (False, False, True),
                                    (True, False, True)):
        fn = original
        if early:
            assert fn.count('\tk = 0;') == 1
            fn = fn.replace('\tk = 0;\n', '')
            fn = fn.replace('\tshort i, j, k;', '\tshort i, j, k = 0;')
        if separate:
            old = ('\t\tfor (i = j; i >= 0; i -= V22_MRF_PHASES)\n'
                   '\t\t\twork[k++] = coeff[i];')
            new = ('\t\tfor (i = j; i >= 0; i -= V22_MRF_PHASES) {\n'
                   '\t\t\twork[k] = coeff[i];\n\t\t\tk++;\n\t\t}')
            assert fn.count(old) == 1
            fn = fn.replace(old, new)
        if preload:
            old = ('\t\tfor (i = j; i >= 0; i -= V22_MRF_PHASES)\n'
                   '\t\t\twork[k++] = coeff[i];')
            new = ('\t\tfor (i = j; i >= 0; i -= V22_MRF_PHASES) {\n'
                   '\t\t\tshort value = coeff[i];\n'
                   '\t\t\twork[k++] = value;\n\t\t}')
            assert fn.count(old) == 1
            fn = fn.replace(old, new)
        cells['early-%d-separate-%d-preload-%d' % (early, separate, preload)] = source[:start]+fn+source[end:]
    assert len(set(cells.values())) == 6
    return cells

if __name__ == '__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
    d.REV = 'a95a6c65'
    d.OUT_NAME = 'v22-mrf-counter-preload'
    d.SOURCE_PATHS = ('src/pump/v22/v22_mrf.c',)
    d.variants = variants
    d.main()
