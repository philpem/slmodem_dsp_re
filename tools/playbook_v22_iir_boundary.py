#!/usr/bin/env python3
"""Four full-TU conversion/reset-boundary controls for V22 IIR demodulation."""
import playbook_small_patterns as driver
from playbook_v22_iir_narrow import variants as narrow_variants


def variants(path, source):
    cells = narrow_variants(path, source)
    for label, parent in [('boundary-reset', 'baseline'), ('both', 'narrow-at-uses')]:
        text = cells[parent]
        start, end, fn = driver.function(text, 'V22_iir_filt_demod')
        assert fn.count('\t\tint acc = 0;\n') == 1
        changed = fn.replace('\tshort i;\n', '\tshort i;\n\tint acc = 0;\n', 1)
        changed = changed.replace('\t\tint acc = 0;\n', '')
        marker = '\n\t}\n}'
        assert changed.count(marker) == 1
        changed = changed.replace(marker, '\n\t\tacc = 0;\n\t}\n}')
        cells[label] = text[:start] + changed + text[end:]
    assert len(set(cells.values())) == 4
    return cells


if __name__ == '__main__':
    driver.REV = '5ec53f63'
    driver.OUT_NAME = 'playbook-v22-iir-boundary'
    driver.SOURCE_PATHS = ('src/pump/v22/v22_iir.c',)
    driver.variants = variants
    driver.main()
