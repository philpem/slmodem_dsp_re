#!/usr/bin/env python3
"""Reproduce unchanged Queue/Echo TUs for installed-compiler observations.

Use --domain queue-echo-baseline --baseline-dir build/tc_repro. The shared
driver verifies raw complete-object identity, compiler/assembler provenance,
headers and reproduction flags before the observations are interpreted.
"""
import playbook_small_patterns as d

d.REV = '06ba1f713540f85bafba1901aa94628ac37b7a55'
d.OUT_NAME = 'gcc3-reload-baseline'
d.SOURCE_PATHS = ('src/service/Fdspkrnl.c', 'src/pump/v90/V92Modulator.cpp')
d.DUMP_FLAGS = ('-da', '-v', '-save-temps')
d.variants = lambda path, source: {'baseline': source}

if __name__ == '__main__':
    d.main()
