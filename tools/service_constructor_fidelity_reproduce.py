#!/usr/bin/env python3
"""Current fixed controls for wrapper failure and cadence behaviour boundaries."""
import playbook_small_patterns as d
from gcc3_batch50_wrapper_dispatch import variants as dispatch
from gcc3_batch50_wrapper_cleanup import variants as cleanup
from gcc3_batch100_callprog_cadence_banks import variants as banks
from gcc3_batch100_callprog_cadence_interior import variants as interior


def variants(path, source):
    if path.endswith('/dp_wrapper.c'):
        return {'baseline': source,
                'sequential-failure': dispatch(path, source)['literal-0-sequential-1'],
                'literal-shared-failure': cleanup(path, source)['creation-helper-delete-first']}
    bank = banks(path, source)
    cells = {'baseline': source, 'corrected-bank-table': bank['table-original-map'],
             'original-switch-map': bank['switch-original-map']}
    for label, text in list(cells.items()):
        cells['interior-on' if label == 'baseline' else label+'-interior-on'] = interior(path, text)['interior-on-comparison']
    assert len(cells) == 6
    return cells


if __name__ == '__main__':
    d.REV = '18f4ad68'
    d.SOURCE_PATHS = ('src/core/dp_wrapper.c', 'src/callprog/Cadence.c')
    d.OUT_NAME = 'service-constructor-fidelity'
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da')
    d.variants = variants
    d.main()
