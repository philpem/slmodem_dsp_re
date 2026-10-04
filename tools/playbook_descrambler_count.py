#!/usr/bin/env python3
"""Descrambler ctor count-temp family on all defining TUs.

Posted as the "Descrambler ctor count-temp family" comment in #22. All
cells allocate the same number of elements; only the tree association
varies. No fuzzing or mutation execution.
"""
import playbook_small_patterns as driver

REV = '0056f610'
OUT_NAME = 'playbook-descrambler-count'
DUMP_FLAGS = ()
# Every TU that instantiates Descrambler with sizeof(T) > 1 or shares the header.
SOURCE_PATHS = ('src/pump/v90/V90Demodulator.cpp',
                'src/pump/v90/V90Phase3Demodulator.cpp')

OLD = '\tpLimit = (T *)sysdep_malloc((1 + b + c) * sizeof(T));\n'


def overlay(label, text):
    assert text.count(OLD) == 1
    if label == 'count-temp-bc1':
        new = ('\tunsigned int count = b + c + 1;\n'
               '\tpLimit = (T *)sysdep_malloc(count * sizeof(T));\n')
    elif label == 'count-temp-1bc':
        new = ('\tunsigned int count = 1 + b + c;\n'
               '\tpLimit = (T *)sysdep_malloc(count * sizeof(T));\n')
    else:
        return text
    return text.replace(OLD, new)


def variants(path, source):
    return {label: source for label in
            ('baseline', 'count-temp-bc1', 'count-temp-1bc')}


def header_overlays(path, label):
    if label == 'baseline':
        return {}
    import subprocess
    text = subprocess.check_output(
        ['git', 'show', REV + ':include/dsplib/Scrambler.h'],
        cwd=driver.ROOT, text=True)
    return {'dsplib/Scrambler.h': overlay(label, text)}


driver.REV = REV
driver.OUT_NAME = OUT_NAME
driver.DUMP_FLAGS = DUMP_FLAGS
driver.SOURCE_PATHS = SOURCE_PATHS
driver.variants = variants
driver.HEADER_OVERLAYS = header_overlays

if __name__ == '__main__':
    driver.main()
