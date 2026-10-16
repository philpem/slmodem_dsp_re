#!/usr/bin/env python3
"""Restore the original guarded B103 constructor version diagnostic."""
import playbook_small_patterns as d


def variants(path, source):
    start, end, body = d.function(source, 'B103FP_create')
    old = '\n\treturn fp;'
    assert body.count(old) == 1
    new = '\n\tif (DSPLIB_DEBUG_ON())\n\t\tdsplibs_debug_printf("B103FP version: %s %s\\n",\n\t\t\t\t     "Sep 22 2005", "15:48:11");\n' + old
    return {'baseline': source, 'original-banner': source[:start] + body.replace(old, new) + source[end:]}


if __name__ == '__main__':
    d.REV = 'b856fde4'
    d.SOURCE_PATHS = ('src/pump/b103/B103prc.c',)
    d.OUT_NAME = 'stage-c-b103-banner'
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dP')
    d.variants = variants
    d.main()
