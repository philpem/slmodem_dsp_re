#!/usr/bin/env python3
"""Two independent shared counter sinks and their combination per receiver TU."""
import playbook_small_patterns as d

def variants(path, source):
    family = 'V27' if path.endswith('V27rx.c') else 'V29'
    wrap = 'V27DEC_PHASE_FULL' if family == 'V27' else 'V29DEC_SYM_COUNT_WRAP'
    restart = 'V27DEC_PHASE_FULL / 2' if family == 'V27' else 'V29DEC_SYM_COUNT_RESTART'
    cells = {}
    for eq, decision in [(0, 0), (1, 0), (0, 1), (1, 1)]:
        text = source
        for enabled, suffix in [(eq, '_eq_train'), (decision, '_decision')]:
            if not enabled:
                continue
            a, z, fn = d.function(text, family+'RX'+suffix)
            old = ('\tif (count == '+wrap+')\n\t\tdec->sym_count = '+restart+
                   ';\n\telse\n\t\tdec->sym_count = count;')
            assert fn.count(old) == 1
            new = '\tdec->sym_count = count == '+wrap+' ? '+restart+' : count;'
            text = text[:a]+fn.replace(old, new)+text[z:]
        cells['baseline' if not (eq or decision) else f'eq-{eq}-decision-{decision}'] = text
    assert len(set(cells.values())) == 4
    return cells

if __name__ == '__main__':
    d.REV = '04eee73f'
    d.OUT_NAME = 'shared-counter-results'
    d.SOURCE_PATHS = ('src/fax/V27rx.c', 'src/fax/V29rx.c')
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dP')
    d.variants = variants
    d.main()
