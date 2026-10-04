#!/usr/bin/env python3
"""Bound constructor field ownership against the original count/length stores.

Prediction: grouping each count with its derived length shortens blockSize's
live range and removes the extra saved ESI. The original stores denominator
count/length first, then numerator count/length and borrowed coefficients.
This is a three-cell source domain, not arbitrary store-order enumeration.
"""
import playbook_small_patterns as d

def variants(path, source):
    old = '\tm_den = den;\n\tm_num = num;\n\tm_nden = nden;\n\tm_nnum = nnum;'
    lengths = '\tm_inLen = nnum + blockSize;\n\tm_outLen = nden + blockSize;'
    assert source.count(old) == source.count(lengths) == 1
    grouped = ('\tm_nden = nden;\n\tm_outLen = nden + blockSize;\n'
               '\tm_nnum = nnum;\n\tm_inLen = nnum + blockSize;\n'
               '\tm_den = den;\n\tm_num = num;')
    early = source.replace(old, grouped).replace(lengths, '')
    # Cross the same count/length grouping with retained early coefficient
    # ownership; these stores all precede both allocator calls.
    coefficients = early.replace(grouped, '\tm_den = den;\n\tm_num = num;\n' +
                                 grouped.replace('\tm_den = den;\n\tm_num = num;', ''))
    cells = {'baseline': source, 'count-length-owned': early,
             'coefficients-early': coefficients}
    assert len(set(cells.values())) == 3
    return cells

if __name__ == '__main__':
    d.REV = '14769433'
    d.SOURCE_PATHS = ('src/dsp/FloatIIR.cpp',)
    d.OUT_NAME = 'gcc3-iir-constructor'
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da')
    d.variants = variants
    d.main()
