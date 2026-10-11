#!/usr/bin/env python3
"""Prepare ADID spill observations and a bounded emission-context diagnostic."""
import sys
from pathlib import Path
import playbook_small_patterns as d
from gcc3_adid_phase_grouping import variants as guards
from gcc3_adid_phase_definition_age import variants as definitions


def move_control(source):
    a, z, fn = d.function(source, 'V90AutoDigitalImpDetector::updateUref')
    # Move the intact exact helper body past the target: a mechanism control,
    # never an inference about the original author's definition order.
    prefix = source.rfind('\nvoid\n', 0, a)
    assert prefix >= 0 and source[prefix:a] == '\nvoid\n'
    block = source[prefix:z]
    source = source[:prefix] + source[z:]
    _, end, _ = d.function(source, 'V90AutoDigitalImpDetector::uniteLinMappInfoOfUnsuspectedPhases')
    return source[:end] + '\n' + block + source[end:]


def variants(path, source):
    guard = guards(path, source)['late-1-group-1-pool-1']
    closest = definitions(path, source)['best-first-0-float-first-1']
    return {'baseline': source, 'guard-joins': guard, 'closest': closest,
            'closest-context': move_control(closest)}


if __name__ == '__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain') + 1]).is_file()
    d.REV = '22f4a724'
    d.SOURCE_PATHS = ('src/pump/v90/V90AutoDigitalImpDetector.cpp',)
    d.OUT_NAME = 'gcc3-adid-spill-context'
    d.DUMP_FLAGS = ('-v', '-save-temps', '-dr', '-dc', '-dg')
    d.variants = variants
    d.main()
