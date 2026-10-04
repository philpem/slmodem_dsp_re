#!/usr/bin/env python3
"""Scrambler ctor tap-pointer spelling family on both defining TUs.

Posted as the "Scrambler ctor tap-pointer family" comment in #22. All cells
compute identical pointers; only the tree association and statement order
vary. No fuzzing or mutation execution.
"""
import playbook_small_patterns as driver

REV = '2185e6b5'
OUT_NAME = 'playbook-scrambler-taps'
DUMP_FLAGS = ()
SOURCE_PATHS = ('src/pump/v90/V90Phase4Modulator.cpp',
                'src/pump/v90/V90Phase3Modulator.cpp')

OLD = ('\tpInitTap1 = pInitOut + a;\n'
       '\tpInitTap2 = pInitOut + b;')


def overlay(label, text):
    # Descrambler's ctor repeats the block; only Scrambler's is in domain.
    i = text.index(OLD)
    j = text.index(OLD, i + 1)
    assert text.index('Scrambler<T, I>::Scrambler') < i < j
    if label == 'tap-direct':
        new = '\tpInitTap1 = pLimit + c + a;\n\tpInitTap2 = pLimit + c + b;'
    elif label == 'tap-swap':
        new = '\tpInitTap2 = pInitOut + b;\n\tpInitTap1 = pInitOut + a;'
    else:
        return text
    return text[:i] + new + text[i + len(OLD):]


def variants(path, source):
    return {label: source for label in
            ('baseline', 'tap-direct', 'tap-swap')}


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
