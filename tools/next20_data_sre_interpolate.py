#!/usr/bin/env python3
"""Original intermediate coefficient store before the second prototype read."""
import playbook_small_patterns as d

def variants(path, source):
    a, z, fn = d.function(source, 'sre_interpolate')
    old = ('\t\t\t/*\n\t\t\t * The original stores the first term before adding the\n'
           '\t\t\t * second, to the same slot; that store is dead and is\n'
           '\t\t\t * not reproduced.\n\t\t\t */\n'
           '\t\t\tint v = (SREv22_COFFS[k] * w0) >> 15;\n\n'
           '\t\t\tv += (SREv22_COFFS[k + 1] * w1) >> 15;\n'
           '\t\t\tco[j++] = (short)v;')
    assert fn.count(old) == 1
    new = ('\t\t\tco[j] = (short)((SREv22_COFFS[k] * w0) >> 15);\n'
           '\t\t\tco[j] += (short)((SREv22_COFFS[k + 1] * w1) >> 15);\n'
           '\t\t\tj++;')
    return {'baseline': source, 'first-term-store': source[:a] + fn.replace(old, new) + source[z:]}

if __name__ == '__main__':
    d.REV = '8af3af53'
    d.SOURCE_PATHS = ('src/pump/v22/v22_sre.c',)
    d.OUT_NAME = 'next20-data-sre-interpolate'
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da')
    d.variants = variants
    d.main()
