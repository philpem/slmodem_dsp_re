#!/usr/bin/env python3
"""V90RDetector Not pair: sign-register reload and named-return family.

Posted as the "V90RDetector Not pair" domain comment in #22. All cells are
value-identical spellings of detectRNot/detectRfNot only; the plain pair and
the five exact siblings are untouched bystanders. No fuzzing or mutation
execution.
"""
import playbook_small_patterns as driver

REV = 'c1617cf8'
OUT_NAME = 'playbook-v90rdetector'
DUMP_FLAGS = ()
SOURCE_PATHS = ('src/pump/v90/V90RDetector.cpp',)

RNOT = 'V90RDetector::detectRNot(short sample)'
RFNOT = 'V90RDetector::detectRfNot(short sample)'

# Boolean lifetime: the blob's != 6 early return copies the verdict variable's
# home (mov %ebx,%eax) where a literal `return 0;` spells xor %eax,%eax.
VERDICT = [('\t\treturn 0;\n', '\t\treturn verdict;\n')]
ANSWER = [('\t\treturn 0;\n', '\t\treturn answer;\n')]

# Access order: the counter read before the signBits store.
COUNTER_NOT = [(
    '\tsignBits = (unsigned short)reg;\n\n\tused = sampleCount + 1;\n',
    '\tused = sampleCount + 1;\n\tsignBits = (unsigned short)reg;\n')]
COUNTER_RFNOT = [(
    '\tsignBits = (unsigned short)shifted;\n\n\tcount = sampleCount + 1;\n',
    '\tcount = sampleCount + 1;\n\tsignBits = (unsigned short)shifted;\n')]

# Arm order: polarity test outside, one pattern compare per arm.
NESTED_NOT = [(
    '\tif ((unsigned int)signBits == (polarity > 0 ? 0x07u : 0x38u)) {\n'
    '\t\tnotRunLength += 6;\n'
    '\t\tif (notRunLength == rNotLimit)\n'
    '\t\t\tverdict = -1;\n'
    '\t} else {\n'
    '\t\tnotRunLength = 0;\n'
    '\t}\n',
    '\tif (polarity > 0) {\n'
    '\t\tif ((unsigned int)signBits == 0x07u) {\n'
    '\t\t\tnotRunLength += 6;\n'
    '\t\t\tif (notRunLength == rNotLimit)\n'
    '\t\t\t\tverdict = -1;\n'
    '\t\t} else {\n'
    '\t\t\tnotRunLength = 0;\n'
    '\t\t}\n'
    '\t} else {\n'
    '\t\tif ((unsigned int)signBits == 0x38u) {\n'
    '\t\t\tnotRunLength += 6;\n'
    '\t\t\tif (notRunLength == rNotLimit)\n'
    '\t\t\t\tverdict = -1;\n'
    '\t\t} else {\n'
    '\t\t\tnotRunLength = 0;\n'
    '\t\t}\n'
    '\t}\n')]
NESTED_RFNOT = [(
    '\tif ((unsigned int)signBits == (polarity > 0 ? 0x333u : 0xcccu)) {\n'
    '\t\tnotRunLength += 12;\n'
    '\t\tif (notRunLength == rfNotLimit)\n'
    '\t\t\tanswer = -1;\n'
    '\t} else {\n'
    '\t\tnotRunLength = 0;\n'
    '\t}\n',
    '\tif (polarity > 0) {\n'
    '\t\tif ((unsigned int)signBits == 0x333u) {\n'
    '\t\t\tnotRunLength += 12;\n'
    '\t\t\tif (notRunLength == rfNotLimit)\n'
    '\t\t\t\tanswer = -1;\n'
    '\t\t} else {\n'
    '\t\t\tnotRunLength = 0;\n'
    '\t\t}\n'
    '\t} else {\n'
    '\t\tif ((unsigned int)signBits == 0xcccu) {\n'
    '\t\t\tnotRunLength += 12;\n'
    '\t\t\tif (notRunLength == rfNotLimit)\n'
    '\t\t\t\tanswer = -1;\n'
    '\t\t} else {\n'
    '\t\t\tnotRunLength = 0;\n'
    '\t\t}\n'
    '\t}\n')]


def edit_fn(source, marker, pairs):
    start = source.index(marker)
    end = source.index('\n}\n', start) + 3
    body = source[start:end]
    for old, new in pairs:
        assert body.count(old) == 1, (marker, old)
        body = body.replace(old, new)
    return source[:start] + body + source[end:]


def pair_edits(source, not_pairs, rfnot_pairs):
    source = edit_fn(source, RNOT, not_pairs)
    return edit_fn(source, RFNOT, rfnot_pairs)


def variants(path, source):
    v = pair_edits(source, VERDICT, ANSWER)
    c = pair_edits(source, COUNTER_NOT, COUNTER_RFNOT)
    n = pair_edits(source, NESTED_NOT, NESTED_RFNOT)
    cells = {
        'baseline': source,
        'verdict-return': v,
        'counter-first': c,
        'verdict+counter': pair_edits(v, COUNTER_NOT, COUNTER_RFNOT),
        'nested-arms': n,
        'verdict+nested-arms': pair_edits(v, NESTED_NOT, NESTED_RFNOT),
    }
    assert len(cells) == 6 and len(set(cells.values())) == 6
    return cells


driver.REV = REV
driver.OUT_NAME = OUT_NAME
driver.DUMP_FLAGS = DUMP_FLAGS
driver.SOURCE_PATHS = SOURCE_PATHS
driver.variants = variants

if __name__ == '__main__':
    driver.main()
