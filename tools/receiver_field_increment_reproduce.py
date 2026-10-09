#!/usr/bin/env python3
"""Counter field increment boundary, separately from conditional-value sharing."""
import re
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
            old = ('\tcount = (unsigned short)(dec->sym_count + 1);\n\tif (count == '+wrap+
                   ')\n\t\tdec->sym_count = '+restart+';\n\telse\n\t\tdec->sym_count = count;')
            assert fn.count(old) == fn.count('\tunsigned short count;\n') == 1
            new = '\t++dec->sym_count;\n\tif (dec->sym_count == '+wrap+')\n\t\tdec->sym_count = '+restart+';'
            fn = fn.replace(old, new).replace('\tunsigned short count;\n', '')
            assert not re.search(r'\bcount\b', re.sub(r'/\*.*?\*/', '', fn, flags=re.S))
            text = text[:a]+fn+text[z:]
        cells['baseline' if not (eq or decision) else f'eq-{eq}-decision-{decision}'] = text
    return cells

if __name__ == '__main__':
    d.REV = '04eee73f'
    d.OUT_NAME = 'receiver-field-increment'
    d.SOURCE_PATHS = ('src/fax/V27rx.c', 'src/fax/V29rx.c')
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dP')
    d.variants = variants
    d.main()
