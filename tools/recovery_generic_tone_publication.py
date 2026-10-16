#!/usr/bin/env python3
"""Replay witnessed output publication and scalar decision lifetimes."""
import itertools
import playbook_small_patterns as driver


def variants(path, source):
    cells = {}
    for output_first, early_predicate in itertools.product((False, True), repeat=2):
        text = source
        if output_first:
            for indent in ('\t\t', '\t\t\t'):
                old = indent + 'acc_0c = in;\n' + indent + 'acc_10 = out;'
                new = indent + 'acc_10 = out;\n' + indent + 'acc_0c = in;'
                assert text.count(old) == 1
                text = text.replace(old, new)
        if early_predicate:
            start = text.index('int GenericToneDetector::process(float sample)')
            end = text.index('\n}\n', start) + 2
            function = text[start:end]
            marker = '\t\tacc_14 = 0.7f * acc_14 + 0.3f * meanIn;'
            assert function.count(marker) == 1
            function = function.replace(marker,
                                        '\t\tbool strong = meanOut >= threshold;\n\n' + marker)
            assert function.count('if (meanOut >= threshold)') == 1
            function = function.replace('if (meanOut >= threshold)', 'if (strong)')
            text = text[:start] + function + text[end:]
        label = '-'.join(name for name, enabled in
                         (('output-first', output_first), ('early-predicate', early_predicate))
                         if enabled) or 'baseline'
        cells[label] = text
    assert len(cells) == len(set(cells.values())) == 4
    return cells


if __name__ == '__main__':
    driver.REV = '60277060'
    driver.SOURCE_PATHS = ('src/dsp/GenericToneDetector.cpp',)
    driver.OUT_NAME = 'recovery-generic-tone-publication'
    driver.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dP')
    driver.variants = variants
    driver.main()
