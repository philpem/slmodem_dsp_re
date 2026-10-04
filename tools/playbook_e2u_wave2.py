#!/usr/bin/env python3
"""Wave-2 follow-up A: generateE2u's one untested cell — a second 16-bit
lvalue whose stack slot coalesces with sym's.

Path-1-only `short sym`; late-only `unsigned short u` reader; the late call
takes `(short *)&u`.  Scratch pre-screen (11 chain spellings, period GCC
3.4.2-r2): every composed chain emits movswl; the plain unsigned return emits
the zero-extending load with NO cwde.  The real-TU cell decides the one
question the scratch cannot: whether the slots coalesce inside this frame.
"""
import playbook_small_patterns as driver

REV = '17d182af'
OUT_NAME = 'playbook-e2u-final'
DUMP_FLAGS = ()
SOURCE_PATHS = ('src/pump/v90/V92Phase4Modulator.cpp',)

E2U = 'int V92Phase4Modulator::generateE2u()'
DECL = '\tunsigned int last;\n\tshort sym;\n'
CALL_RET = '\tbitsToSymbol->process(n, &sym);\n\treturn sym;\n'


def variants(path, source):
    start = source.index(E2U)
    end = source.index('\n}\n', start) + 3
    head, fn, tail = source[:start], source[start:end], source[end:]
    assert fn.count(DECL) == 1
    assert fn.count(CALL_RET) == 1
    cells = {'baseline': source}
    # a1: path-1-only short sym, late-only unsigned reader.  The early path
    # is untouched; the late paths read through the unsigned lvalue.
    a1 = fn.replace(DECL, '\tunsigned int last;\n\tshort sym;\n'
                          '\tunsigned short u;\n')
    a1 = a1.replace(CALL_RET, '\tbitsToSymbol->process(n, (short *)&u);\n'
                              '\treturn u;\n')
    cells['late-unsigned-reader'] = head + a1 + tail
    assert len(cells) == 2
    return cells


driver.REV = REV
driver.OUT_NAME = OUT_NAME
driver.DUMP_FLAGS = DUMP_FLAGS
driver.SOURCE_PATHS = SOURCE_PATHS
driver.variants = variants

if __name__ == '__main__':
    driver.main()
