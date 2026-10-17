#!/usr/bin/env python3
"""Replay the two independently witnessed echo diagnostic first bytes."""
import playbook_small_patterns as d


def variants(path, source):
    candidate = source
    for message in ('ON', 'Off'):
        old = '" *** Echo Silence ' + message + ' *** \\n"'
        assert candidate.count(old) == 1
        candidate = candidate.replace(old, '"\\n *** Echo Silence ' + message + ' *** \\n"')
    return {'baseline': source, 'original-newlines': candidate}


if __name__ == '__main__':
    d.REV = 'e0e56d23'
    d.SOURCE_PATHS = ('src/pump/v90/VpcmFloModem.cpp',)
    d.OUT_NAME = 'stage-c-echo-newline'
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dP')
    d.variants = variants
    d.main()
