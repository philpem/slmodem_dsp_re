#!/usr/bin/env python3
"""Test the original threshold decision before smoother publications."""
import playbook_small_patterns as driver


def variants(path, source):
    start = source.index('int GenericToneDetector::process(float sample)')
    end = source.index('\n}\n', start) + 2
    function = source[start:end]
    cells = {'baseline': source}
    for both in (False, True):
        updates = ('\t\tacc_14 = 0.7f * acc_14 + 0.3f * meanIn;\n' if both else '')
        updates += '\t\tacc_18 = 0.7f * acc_18 + 0.3f * meanOut;\n'
        assert function.count(updates) == 1
        changed = function.replace(updates, '')
        inside = ''.join('\t' + line + '\n' for line in updates.splitlines())
        marker = '\t\tif (meanOut >= threshold) {\n'
        assert changed.count(marker) == 1
        changed = changed.replace(marker, marker + inside)
        # Select only the outer weak arm; the strong ratio-miss arm differs
        # in indentation and is preserved exactly.
        old = '\t\t} else if (++count_30 >= blocks2) {\n\t\t\tcount_2c = 0;\n\t\t\tdetected = 0;\n\t\t}'
        assert changed.count(old) == 1
        new = '\t\t} else {\n' + inside + '\t\t\tif (++count_30 >= blocks2) {\n\t\t\t\tcount_2c = 0;\n\t\t\t\tdetected = 0;\n\t\t\t}\n\t\t}'
        changed = changed.replace(old, new)
        cells['both-smoothers-in-arms' if both else 'output-smoother-in-arms'] = source[:start] + changed + source[end:]
    # Historical mean-width control is rerun only to discriminate the
    # independently observed current threshold load/operand boundary.
    assert function.count('float meanOut = out * inv;') == 1
    changed = function.replace('float meanOut = out * inv;', 'double meanOut = out * inv;')
    cells['double-output-mean-control'] = source[:start] + changed + source[end:]
    return cells


if __name__ == '__main__':
    driver.REV = 'a2d88616'
    driver.SOURCE_PATHS = ('src/dsp/GenericToneDetector.cpp',)
    driver.OUT_NAME = 'recovery-generic-tone-predicate-ownership'
    driver.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dP')
    driver.variants = variants
    driver.main()
