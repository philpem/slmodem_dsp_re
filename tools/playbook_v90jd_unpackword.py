#!/usr/bin/env python3
"""V90Jd ctor unpackWord store-position family.

Posted as the "V90Jd::C2 store-position family" comment in #22. All cells
reorder independent stores of the constructor body; behavior is identical.
No fuzzing or mutation execution.
"""
import playbook_small_patterns as driver

REV = '0056f610'
OUT_NAME = 'playbook-v90jd-temps'
DUMP_FLAGS = ()
SOURCE_PATHS = ('src/pump/v90/V90Jd.cpp',)

UNPACKWORD = '\tunpackWord = 0;\n'
LOOK = '\tlook = (unsigned char)params->MAX_SPECTRAL_SHAPER_LOOKAHEAD;\n'
BITS50 = '\tbits[50] = (unsigned char)((look >> 1) & 1);\n'
CONST = '\tbits[48] = (unsigned char)params->V34_RRN_CONSTELLATION;\n'


def variants(path, source):
    # V90Jd::reset also assigns unpackWord; scope every edit to the ctor.
    ctor_start = source.index('V90Jd::V90Jd(V90Parameters *params)')
    ctor_end = source.index('\n}\n', ctor_start) + 3
    head, ctor, tail = (source[:ctor_start], source[ctor_start:ctor_end],
                        source[ctor_end:])
    assert ctor.count(UNPACKWORD) == 1
    assert ctor.count(LOOK) == 1
    assert ctor.count(BITS50) == 1
    assert ctor.count(CONST) == 1
    TEMPS_OLD = ('\tbits[47] = (unsigned char)params->V34_PHASE4_CONSTELLATION;\n'
                 '\tbits[48] = (unsigned char)params->V34_RRN_CONSTELLATION;\n')
    TEMPS_NEW = ('\tint c47 = params->V34_PHASE4_CONSTELLATION;\n'
                 '\tint c48 = params->V34_RRN_CONSTELLATION;\n'
                 '\tbits[47] = (unsigned char)c47;\n'
                 '\tbits[48] = (unsigned char)c48;\n')
    assert ctor.count(TEMPS_OLD) == 1
    temps = ctor.replace(TEMPS_OLD, TEMPS_NEW)
    stripped_t = temps.replace(UNPACKWORD, '')
    look_anchor = '\tlook = (unsigned char)params->MAX_SPECTRAL_SHAPER_LOOKAHEAD;\n'
    cells = {'baseline': head + ctor + tail,
             'temp': head + temps + tail,
             'temp-after-look': (head + stripped_t.replace(
                 look_anchor, look_anchor + '\n' + UNPACKWORD) + tail)}
    assert len(cells) == 3
    return cells


driver.REV = REV
driver.OUT_NAME = OUT_NAME
driver.DUMP_FLAGS = DUMP_FLAGS
driver.SOURCE_PATHS = SOURCE_PATHS
driver.variants = variants

if __name__ == '__main__':
    driver.main()
