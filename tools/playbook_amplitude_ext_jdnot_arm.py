#!/usr/bin/env python3
"""amplitude read-extension family + JdNotDetector arm-order family.

Posted as the "two more bounded data-mode domains" comment in #22. All cells
are value-identical: the cast re-reads the same 16 bits, the arm swap keeps
the same store on each side. No fuzzing or mutation execution.
"""
import playbook_small_patterns as driver

REV = '2185e6b5'
OUT_NAME = 'playbook-amplitude-ext-jdnot-arm'
DUMP_FLAGS = ()
SOURCE_PATHS = ('src/pump/v90/V92Phase4Modulator.cpp',
                'src/pump/v90/V90Phase3Demodulator.cpp')

CP_OLD = '\tsym = amplitude;\n\tif (bit != 0)\n\t\tsym = -sym;\n\treturn sym;'
E1U_OLD = '\tsym = amplitude;\n\tif (bit != 0)\n\t\tsym = -sym;\n\treturn sym;'
JD_OLD = ('\tif (symbol == 0)\n\t\tjdNotRunLength++;\n'
          '\telse\n\t\tjdNotRunLength = 0;')


def v92_variants(source):
    assert source.count(CP_OLD) == 2  # generateCPt and generateE1u
    casted = source.replace(CP_OLD,
                            '\tsym = (unsigned short)amplitude;\n'
                            '\tif (bit != 0)\n\t\tsym = -sym;\n\treturn sym;')
    cells = {'baseline': source, 'cast-both': casted}
    # one-function cells: revert each site in turn
    first = casted.find('\tsym = (unsigned short)amplitude;')
    second = casted.find('\tsym = (unsigned short)amplitude;', first + 1)
    assert first >= 0 and second > first
    cells['cast-e1u-only'] = (casted[:first] + '\tsym = amplitude;' +
                              casted[first + len('\tsym = (unsigned short)amplitude;'):])
    cells['cast-cpt-only'] = (casted[:second] + '\tsym = amplitude;' +
                              casted[second + len('\tsym = (unsigned short)amplitude;'):])
    assert len(cells) == 4
    return cells


def p3d_variants(source):
    assert source.count(JD_OLD) == 1
    cells = {'baseline': source}
    cells['jd-arm-swap'] = source.replace(
        JD_OLD,
        '\tif (symbol != 0)\n\t\tjdNotRunLength = 0;\n'
        '\telse\n\t\tjdNotRunLength++;')
    cells['jd-ternary'] = source.replace(
        JD_OLD, '\tjdNotRunLength = (symbol == 0) ? jdNotRunLength + 1 : 0;')
    return cells


def variants(path, source):
    if 'V92Phase4Modulator' in path:
        return v92_variants(source)
    return p3d_variants(source)


driver.REV = REV
driver.OUT_NAME = OUT_NAME
driver.DUMP_FLAGS = DUMP_FLAGS
driver.SOURCE_PATHS = SOURCE_PATHS
driver.variants = variants

if __name__ == '__main__':
    driver.main()
