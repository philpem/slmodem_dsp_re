#!/usr/bin/env python3
"""Reproduce complete TUs for the three data-service diagnostic-count leads."""
import playbook_small_patterns as d

d.REV = '95e01c27'
d.OUT_NAME = 'diagnostic-audit-next'
d.SOURCE_PATHS = ('src/pump/b103/B103prc.c', 'src/service/Rxcid.c', 'src/v8/V8.c')
d.DUMP_FLAGS = ('-da', '-fdump-tree-original')
d.variants = lambda path, source: {'baseline': source}
if __name__ == '__main__':
    d.main()
