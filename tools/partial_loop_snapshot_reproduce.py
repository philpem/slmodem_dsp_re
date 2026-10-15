#!/usr/bin/env python3
"""Review two previously measured operand boundaries for partial adoption."""
import playbook_small_patterns as d
from batch50_fax_v29_control import variants as control
from gcc3_fskdetect_phase_read_age import variants as fsk


def variants(path, source):
    if path.endswith('/V29r_stc.c'):
        cells = control(path, source)
        return {k: cells[k] for k in ('baseline', 'both-snapshots')}
    cells = fsk(path, source)
    return {k: cells[k] for k in ('baseline', 'transfer-1-tap-1-word-1-coefficient-before-store')}


if __name__ == '__main__':
    d.REV = '1b0a3097'
    d.SOURCE_PATHS = ('src/fax/V29r_stc.c', 'src/pump/v34/DPSK.c')
    d.OUT_NAME = 'partial-loop-snapshot'
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da')
    d.variants = variants
    d.main()
