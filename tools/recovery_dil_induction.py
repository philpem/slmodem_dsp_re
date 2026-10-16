#!/usr/bin/env python3
"""Isolate one induction mechanism with a bounded source/option cross."""
import sys
from pathlib import Path
import playbook_small_patterns as d
from recovery_dil_topology import PATHS
from recovery_dil_regions import variants as regions


def variants(path, source):
    prepared = regions(path, source)
    helper = prepared['segments-0-headers-1']
    direct = prepared['segments-1-headers-1']
    return {'baseline': helper, 'helper-no-strength': helper,
            'direct-retained': direct, 'direct-no-strength': direct}


if __name__ == '__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain') + 1]).is_file()
    d.REV = '4169cb3b'
    d.OUT_NAME = 'recovery-dil-induction'
    d.SOURCE_PATHS = PATHS
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da')
    d.CELL_FLAGS = lambda path, label: ('-fno-strength-reduce',) if label.endswith('no-strength') else ()
    d.variants = variants
    d.main()
