#!/usr/bin/env python3
"""Isolate the measured ce2 speculation with a paired source/profile cross."""
import playbook_small_patterns as d
from recovery_dil_next_info import REV, PATHS

PROFILES = {'retained': (), 'no-if1': ('-fno-if-conversion',),
            'no-if2': ('-fno-if-conversion2',),
            'no-both': ('-fno-if-conversion', '-fno-if-conversion2')}


def variants(path, source):
    forms = {'cached': source}
    if 'V90DIL' in path:
        old = '''\tfloat scaled = (float)(short)n * reciprocal;

\tif (scaled > 0.0f)
\t\treturn (int)(scaled + 0.99999999);
\treturn (int)scaled;'''
        new = '''\treturn (int)((float)(short)n * reciprocal > 0.0f
\t\t? (double)(short)n * reciprocal + 0.99999999
\t\t: (float)(short)n * reciprocal);'''
        assert source.count(old) == 1
        forms['repeated'] = source.replace(old, new)
    return {('baseline' if form == 'cached' and profile == 'retained' else
             form+'-'+profile): text
            for form, text in forms.items() for profile in PROFILES}


def flags(path, label):
    return next((value for key, value in PROFILES.items() if label.endswith(key)), ())


if __name__ == '__main__':
    d.REV = REV
    d.OUT_NAME = 'recovery-dil-next-ifconversion'
    d.SOURCE_PATHS = PATHS
    d.variants = variants
    d.CELL_FLAGS = flags
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da')
    d.main()
