#!/usr/bin/env python3
"""V90RDetector Not pair stage 2: unconditional counter store and single return.

Family 1's folds (verdict-return, counter-first byte-identical to baseline)
killed the named-return and access-order hypotheses: GCC 3.4.2 constant-folds
the early return and reorders the independent load/store freely. The one
explanation left for the blob's memory reload is that the sampleCount store is
on the group path at CSE time -- an unconditional store, and/or a single
return whose value the compiler cannot fold at the merge. All cells are
value-identical spellings of detectRNot/detectRfNot only.
"""
import playbook_small_patterns as driver

REV = 'c1617cf8'
OUT_NAME = 'playbook-v90rdetector-uncond'
DUMP_FLAGS = ()
SOURCE_PATHS = ('src/pump/v90/V90RDetector.cpp',)

RNOT = 'V90RDetector::detectRNot(short sample)'
RFNOT = 'V90RDetector::detectRfNot(short sample)'

COUNTER_NOT = ('\tused = sampleCount + 1;\n'
               '\tif (used != 6) {\n'
               '\t\tsampleCount = used;\n'
               '\t\treturn 0;\n'
               '\t}\n')
COUNTER_RFNOT = ('\tcount = sampleCount + 1;\n'
                 '\tif (count != 12) {\n'
                 '\t\tsampleCount = count;\n'
                 '\t\treturn 0;\n'
                 '\t}\n')

UNCOND_NOT = ('\tused = sampleCount + 1;\n'
              '\tsampleCount = used;\n'
              '\tif (used != 6)\n'
              '\t\treturn 0;\n')
UNCOND_RFNOT = ('\tcount = sampleCount + 1;\n'
                '\tsampleCount = count;\n'
                '\tif (count != 12)\n'
                '\t\treturn 0;\n')

VERDICT_NOT = UNCOND_NOT.replace('return 0;', 'return verdict;')
VERDICT_RFNOT = UNCOND_RFNOT.replace('return 0;', 'return answer;')

GROUP_NOT = ('\tif ((unsigned int)signBits == (polarity > 0 ? 0x07u : 0x38u)) {\n'
             '\t\tnotRunLength += 6;\n'
             '\t\tif (notRunLength == rNotLimit)\n'
             '\t\t\tverdict = -1;\n'
             '\t} else {\n'
             '\t\tnotRunLength = 0;\n'
             '\t}\n'
                '\n'
                '\tsampleCount = 0;\n'
                '\tsignBits = 0;\n'
                '\treturn verdict;\n')
GROUP_RFNOT = ('\tif ((unsigned int)signBits == (polarity > 0 ? 0x333u : 0xcccu)) {\n'
               '\t\tnotRunLength += 12;\n'
               '\t\tif (notRunLength == rfNotLimit)\n'
               '\t\t\tanswer = -1;\n'
               '\t} else {\n'
               '\t\tnotRunLength = 0;\n'
               '\t}\n'
               '\n'
               '\tsampleCount = 0;\n'
               '\tsignBits = 0;\n'
               '\treturn answer;\n')

def reindent(block):
    return block[1:].replace('\n\t', '\n\t\t')


SINGLE_TAIL_NOT = ('\t} else {\n'
                   '\t\tsampleCount = used;\n'
                   '\t}\n'
                   '\treturn verdict;\n')
SINGLE_TAIL_RFNOT = ('\t} else {\n'
                     '\t\tsampleCount = count;\n'
                     '\t}\n'
                     '\treturn answer;\n')

SINGLE_NOT = ('\tif (used == 6) {\n\t\t' + reindent(GROUP_NOT)
              + SINGLE_TAIL_NOT)
SINGLE_RFNOT = ('\tif (count == 12) {\n\t\t' + reindent(GROUP_RFNOT)
                + SINGLE_TAIL_RFNOT)

UNCOND_SINGLE_NOT = ('\tsampleCount = used;\n'
                     '\tif (used == 6) {\n\t\t' + reindent(GROUP_NOT)
                     + '\t}\n'
                     '\treturn verdict;\n')
UNCOND_SINGLE_RFNOT = ('\tsampleCount = count;\n'
                       '\tif (count == 12) {\n\t\t' + reindent(GROUP_RFNOT)
                       + '\t}\n'
               '\treturn answer;\n')


def edit_fn(source, marker, old, new):
    start = source.index(marker)
    end = source.index('\n}\n', start) + 3
    body = source[start:end]
    assert body.count(old) == 1, (marker, old)
    return source[:start] + body.replace(old, new) + source[end:]


def pair(source, not_old, not_new, rf_old, rf_new):
    source = edit_fn(source, RNOT, not_old, not_new)
    return edit_fn(source, RFNOT, rf_old, rf_new)


def variants(path, source):
    cells = {
        'baseline': source,
        'uncond-store': pair(source, COUNTER_NOT, UNCOND_NOT,
                             COUNTER_RFNOT, UNCOND_RFNOT),
        'uncond-store+verdict': pair(source, COUNTER_NOT, VERDICT_NOT,
                                     COUNTER_RFNOT, VERDICT_RFNOT),
        'single-return': pair(source, COUNTER_NOT + '\n' + GROUP_NOT,
                              SINGLE_NOT,
                              COUNTER_RFNOT + '\n' + GROUP_RFNOT,
                              SINGLE_RFNOT),
        'uncond+single-return': pair(source, COUNTER_NOT + '\n' + GROUP_NOT,
                                     UNCOND_SINGLE_NOT,
                                     COUNTER_RFNOT + '\n' + GROUP_RFNOT,
                                     UNCOND_SINGLE_RFNOT),
    }
    assert len(cells) == 5 and len(set(cells.values())) == 5
    return cells


driver.REV = REV
driver.OUT_NAME = OUT_NAME
driver.DUMP_FLAGS = DUMP_FLAGS
driver.SOURCE_PATHS = SOURCE_PATHS
driver.variants = variants

if __name__ == '__main__':
    driver.main()
