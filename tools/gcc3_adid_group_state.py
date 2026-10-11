#!/usr/bin/env python3
"""Test grouping-state declarations at their phase boundary after spill tracing."""
import sys
from pathlib import Path
import playbook_small_patterns as d
from gcc3_adid_spill_context import variants as controls
from gcc3_adid_phase_grouping import NAME


def phase_state(source):
    a, z, fn = d.function(source, NAME)
    for line in ('\tshort best;\n', '\tshort count;\n', '\tshort bestGroup;\n'):
        assert fn.count(line) == 1
        fn = fn.replace(line, '')
    marker = '\tcount = 1;\n\tbest = 0;'
    assert fn.count(marker) == 1
    fn = fn.replace(marker, '\tshort count = 1;\n\tshort best = 0;\n\tshort bestGroup;')
    return source[:a] + fn + source[z:]


def variants(path, source):
    prior = controls(path, source)
    return {'baseline': source, 'guard-joins': prior['guard-joins'],
            'guard-phase-state': phase_state(prior['guard-joins']),
            'closest': prior['closest'],
            'closest-phase-state': phase_state(prior['closest'])}


if __name__ == '__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain') + 1]).is_file()
    d.REV = '22f4a724'
    d.SOURCE_PATHS = ('src/pump/v90/V90AutoDigitalImpDetector.cpp',)
    d.OUT_NAME = 'gcc3-adid-group-state'
    d.DUMP_FLAGS = ('-v', '-save-temps', '-dr', '-dc', '-dg')
    d.variants = variants
    d.main()
