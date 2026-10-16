#!/usr/bin/env python3
"""Separate observed smoother arithmetic from threshold-branch publication."""
import playbook_small_patterns as driver


def variants(path, source):
    start = source.index('int GenericToneDetector::process(float sample)')
    end = source.index('\n}\n', start) + 2
    function = source[start:end]
    updates = '\t\tacc_14 = 0.7f * acc_14 + 0.3f * meanIn;\n\t\tacc_18 = 0.7f * acc_18 + 0.3f * meanOut;'
    assert function.count(updates) == 1
    locals_ = '\t\tfloat nextIn = 0.7f * acc_14 + 0.3f * meanIn;\n\t\tfloat nextOut = 0.7f * acc_18 + 0.3f * meanOut;'
    stores = '\t\tacc_14 = nextIn;\n\t\tacc_18 = nextOut;'
    shared = function.replace(updates, locals_ + '\n' + stores)
    arms = function.replace(updates, locals_)
    marker = '\t\tif (meanOut >= threshold) {\n'
    assert arms.count(marker) == 1
    inner = '\t\t\tacc_14 = nextIn;\n\t\t\tacc_18 = nextOut;\n'
    arms = arms.replace(marker, marker + inner)
    old = '\t\t} else if (++count_30 >= blocks2) {\n\t\t\tcount_2c = 0;\n\t\t\tdetected = 0;\n\t\t}'
    assert arms.count(old) == 1
    arms = arms.replace(old, '\t\t} else {\n' + inner + '\t\t\tif (++count_30 >= blocks2) {\n\t\t\t\tcount_2c = 0;\n\t\t\t\tdetected = 0;\n\t\t\t}\n\t\t}')
    return {'baseline': source,
            'locals-shared-publication': source[:start] + shared + source[end:],
            'locals-arm-publication': source[:start] + arms + source[end:]}


if __name__ == '__main__':
    driver.REV = 'a2d88616'
    driver.SOURCE_PATHS = ('src/dsp/GenericToneDetector.cpp',)
    driver.OUT_NAME = 'recovery-generic-tone-predicate-publication'
    driver.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dP')
    driver.variants = variants
    driver.main()
