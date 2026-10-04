#!/usr/bin/env python3
"""Wave-2 follow-up B: RD_create's one untested switch-tree axis — a switch
with NO default label, the threshold pre-initialized to -3000 before it.

The prior round proved the three arms' PHYSICAL order is a compiler constant
(650,850,1000 ours vs 1000,650,850 blob) invariant under minus4, label swaps,
no-cast and default-first.  Removing the default leaf changes the case tree
and the jump table's out-of-range target; the -3000 store is then either
emitted early (GCC 3.4 does not sink stores) or sunk into the out-of-range
path.  Two positions for the pre-init are measured.
"""
import playbook_small_patterns as driver

REV = '17d182af'
OUT_NAME = 'playbook-rd-nodefault'
DUMP_FLAGS = ()
SOURCE_PATHS = ('src/service/rd.c',)

OLD_SWITCH = (
    '\tswitch ((int)modem_get_param(modem, MDMPRM_CODECTYPE)) {\n'
    '\tcase 4:\n'
    '\tcase 12:\n'
    '\t\tcfg.threshold = 1000;\n'
    '\t\tbreak;\n'
    '\tcase 13:\n'
    '\tcase 15:\n'
    '\t\tcfg.threshold = 650;\n'
    '\t\tbreak;\n'
    '\tcase 14:\n'
    '\t\tcfg.threshold = 850;\n'
    '\t\tbreak;\n'
    '\tdefault:\n'
    '\t\tcfg.threshold = RD_THRESHOLD_DEFAULT;\n'
    '\t\tbreak;\n'
    '\t}\n')

NEW_SWITCH = (
    '\tswitch ((int)modem_get_param(modem, MDMPRM_CODECTYPE)) {\n'
    '\tcase 4:\n'
    '\tcase 12:\n'
    '\t\tcfg.threshold = 1000;\n'
    '\t\tbreak;\n'
    '\tcase 13:\n'
    '\tcase 15:\n'
    '\t\tcfg.threshold = 650;\n'
    '\t\tbreak;\n'
    '\tcase 14:\n'
    '\t\tcfg.threshold = 850;\n'
    '\t\tbreak;\n'
    '\t}\n')

DECLS = '\tstruct rd *rd;\n\tstruct ring_detector_cfg cfg;\n'
PRE_SWITCH = '\tcfg.min_off_dur = 120;\n\n'
INIT = '\tcfg.threshold = RD_THRESHOLD_DEFAULT;\n'


def variants(path, source):
    cells = {'baseline': source}
    assert source.count(OLD_SWITCH) == 1
    assert source.count(DECLS) == 1
    assert source.count(PRE_SWITCH) == 1
    for label, anchor in (
            ('no-default-adjacent', PRE_SWITCH),
            ('no-default-early', DECLS + '\n')):
        text = source.replace(OLD_SWITCH, NEW_SWITCH)
        assert text.count(anchor) == 1
        if anchor is PRE_SWITCH:
            text = text.replace(anchor, anchor + INIT)
        else:
            text = text.replace(anchor, anchor + '\n' + INIT + '\n')
        cells[label] = text
    assert len(cells) == 3
    return cells


driver.REV = REV
driver.OUT_NAME = OUT_NAME
driver.DUMP_FLAGS = DUMP_FLAGS
driver.SOURCE_PATHS = SOURCE_PATHS
driver.variants = variants

if __name__ == '__main__':
    driver.main()
