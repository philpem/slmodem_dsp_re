#!/usr/bin/env python3
"""v8_crc msb spelling family + V92CP definition-order cell.

Follow-up to the "two fresh small data-mode domains" run: the ctor statement
order is settled (order-5 = resetDetector order with byte_04 after zerosRun,
BYTES(8) grade-1 ACCEPT, registers swapped) and the msb signed-read spellings
measured reversed-role. This run enumerates the remaining msb spellings and
tests the Lever-3 carrier: V92CP.cpp definitions reordered to the blob's own
address order. No fuzzing or mutation execution.
"""
import re

import playbook_small_patterns as driver

REV = '2185e6b5'
OUT_NAME = 'playbook-v8crc-msb-v92cp-deforder'
DUMP_FLAGS = ()
SOURCE_PATHS = ('src/v8/V8global.c', 'src/pump/v90/V92CP.cpp')

MSB_OLD = '\tint msb = ((int)(short)crc) < 0 ? 1 : 0;'

BLOB_DEF_ORDER = ['V92CP::~V92CP()', 'V92CP::resetCRC()', 'V92CP::calcCRC()',
                  'V92CP::resetDetector()', 'V92CP::reset()',
                  'V92CP::V92CP()', 'V92CP::setSUV(unsigned int value)',
                  'V92CP::evaluateCRC()', 'float2Bits(float f, unsigned char *bits, int mode)',
                  'V92CP::infoToBits()', 'V92CP::evaluateInfo()',
                  'V92CP::bitsToInfo(unsigned char value)']


def v8_variants(source):
    cells = {'baseline': source}
    assert source.count(MSB_OLD) == 1
    spellings = {
        'msb-narrow-shift': '\tint msb = ((short)crc >> 15) & 1;',
        'msb-compare-value': '\tint msb = ((int)(short)crc) < 0;',
        'msb-unsigned-shift': '\tint msb = (crc >> 15) & 1;',
        'msb-bare-shift': '\tint msb = crc >> 15;',
        'msb-shift31': '\tint msb = (((int)(short)crc) >> 31) & 1;',
    }
    for label, line in spellings.items():
        cells[label] = source.replace(MSB_OLD, line)
    return cells


def split_segments(source):
    """Split into [prefix, seg, seg, ...]; each seg = comments + one def.

    Boundaries are the first definition's start and each top-level close
    brace; names come from column-0 declarators only, so banner text that
    mislabels a neighbour cannot double-name a segment.
    """
    lines = source.split('\n')
    ctor = next(i for i, l in enumerate(lines) if l.startswith('V92CP::V92CP()'))
    pre = max(i for i in range(ctor) if re.match(r'^(#include|#endif)', lines[i]))
    start = pre + 1
    closes = [i for i in range(start, len(lines)) if lines[i] == '}']
    assert closes and closes[0] > start
    starts = [start] + [c + 1 for c in closes[:-1]]
    segs = ['\n'.join(lines[s:e + 1]) for s, e in zip(starts, closes)]
    segs[-1] += '\n' + '\n'.join(lines[closes[-1] + 1:])
    prefix = '\n'.join(lines[:start])
    return prefix, segs


def seg_names(seg):
    m = re.search(r'^V92CP::(~?\w+)', seg, re.M)
    if m:
        key = m.group(1)
        return {'V92CP': 'ctor', '~V92CP': 'D2'}.get(key, key)
    assert re.search(r'^float2Bits', seg, re.M), seg[:80]
    return 'float2Bits'


def v92cp_variants(source):
    prefix, segs = split_segments(source)
    named = [(seg_names(s), s) for s in segs]
    assert len(named) == len(set(n for n, _ in named)), [n for n, _ in named]
    by_name = dict(named)
    current = [n for n, _ in named]
    assert '\n'.join([prefix] + [by_name[n] for n in current]) == source
    order = ['D2' if n in ('D1', 'D2') else n for n in current]
    # blob address order: dtor, resetCRC, calcCRC, resetDetector, reset,
    # ctor, setSUV, evaluateCRC, getBitVector, float2Bits, infoToBits,
    # evaluateInfo, bitsToInfo
    blob = ['D2', 'resetCRC', 'calcCRC', 'resetDetector', 'reset', 'ctor',
            'setSUV', 'evaluateCRC', 'getBitVector', 'float2Bits',
            'infoToBits', 'evaluateInfo', 'bitsToInfo']
    assert sorted(blob) == sorted(order), (sorted(blob), sorted(order))
    cells = {'baseline': source}
    cells['blob-def-order'] = '\n'.join([prefix] + [by_name[n] for n in blob])
    return cells


def variants(path, source):
    if path == 'src/v8/V8global.c':
        return v8_variants(source)
    return v92cp_variants(source)


driver.REV = REV
driver.OUT_NAME = OUT_NAME
driver.DUMP_FLAGS = DUMP_FLAGS
driver.SOURCE_PATHS = SOURCE_PATHS
driver.variants = variants

if __name__ == '__main__':
    driver.main()
