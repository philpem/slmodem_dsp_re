#!/usr/bin/env python3
"""V90Jd ctor: the definition position crossed alone, body held at the decode.

Follows the V92CP cross (playbook_v92cp_cross.py, #22 5971674845/5971702309):
scratch-register assignment for two constants follows statement order x
emission position.  Here the body is already the decoded one (99c51fba,
F11675) and the ctor already sits at the blob's own .text position (4th,
after the three setters -- d8f7171b), so the family brackets the position
domain around that saturated point: first, after the dtor, last.  C1 is the
target (BYTES 34, grade-1: the blob's zero+load1 in %eax, load2 in %edx vs
ours %edx/%eax); C2 and the eleven other exact functions are bystanders.
No fuzzing or mutation execution.
"""
import playbook_small_patterns as driver

REV = 'c1617cf8'
OUT_NAME = 'playbook-v90jd-pos'
DUMP_FLAGS = ()
SOURCE_PATHS = ('src/pump/v90/V90Jd.cpp',)

CTOR_DEF = 'V90Jd::V90Jd(V90Parameters *params)'
SETMAX_BANNER = '/*\n * 0x1e700, 26 bytes.'
DTOR_DEF = 'V90Jd::~V90Jd()'


def variants(path, source):
    assert source.count(CTOR_DEF) == 1
    ctor_start = source.index(CTOR_DEF)
    seg_start = source.rindex('/*', 0, ctor_start)
    ctor_end = source.index('\n}\n', ctor_start) + 3
    seg = source[seg_start:ctor_end]
    assert seg.startswith('/*') and seg.rstrip().endswith('}')
    before, after = source[:seg_start], source[ctor_end:]
    cells = {'baseline': source}

    # ctor-first: above setMaxLookahead's banner, the file's first definition.
    anchor = before.index(SETMAX_BANNER)
    first = before[:anchor] + seg + '\n' + before[anchor:] + after

    # ctor-after-dtor: between ~V90Jd's definition and the accessors' banner.
    dend = after.index('\n}\n', after.index(DTOR_DEF)) + 3
    mid = before + after[:dend] + '\n' + seg + after[dend:]

    # ctor-last: after getBitVector, the file's last definition.
    last = before + after + '\n' + seg

    cells['ctor-first'] = first
    cells['ctor-after-dtor'] = mid
    cells['ctor-last'] = last
    assert len(cells) == len(set(cells.values())) == 4
    for label, text in cells.items():
        assert text.count(CTOR_DEF) == 1, label
        assert text.count(DTOR_DEF) == 1, label
    return cells


driver.REV = REV
driver.OUT_NAME = OUT_NAME
driver.DUMP_FLAGS = DUMP_FLAGS
driver.SOURCE_PATHS = SOURCE_PATHS
driver.variants = variants

if __name__ == '__main__':
    driver.main()
