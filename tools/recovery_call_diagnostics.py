#!/usr/bin/env python3
"""Recover three original call create/run diagnostic sites and message age."""
import playbook_small_patterns as driver


def variants(path, source):
    changed = source
    old = '\tst = sysdep_malloc(sizeof(struct call_dp));'
    assert changed.count(old) == 1
    changed = changed.replace(old, '\tif (DSPLIB_DEBUG_ON())\n\t\tdsplibs_debug_printf("call: create...\\n");\n\n' + old)
    old = '\tif (srate != CALL_NATIVE_RATE) {'
    assert changed.count(old) == 1
    changed = changed.replace(old, old + '\n\t\tif (DSPLIB_DEBUG_ON())\n\t\t\tdsplibs_debug_printf("call: create RC: %d <-> %d...\\n",\n\t\t\t\t\t    srate, CALL_NATIVE_RATE);\n')
    old = '\tst->message = CALLPROG_Progress(&st->callprog, in, out, count);'
    assert changed.count(old) == 1
    changed = changed.replace(old, '\tmessage = CALLPROG_Progress(&st->callprog, in, out, count);')
    old = '\tint count;\n\tint rc;'
    assert changed.count(old) == 1
    changed = changed.replace(old, old + '\n\tint message;')
    old = '\trc = call_status(st, st->message);'
    assert changed.count(old) == 1
    changed = changed.replace(old, '\tif (message != st->message) {\n\t\tif (DSPLIB_DEBUG_ON())\n\t\t\tdsplibs_debug_printf("call: process: msg %d --> %d\\n",\n\t\t\t\t\t    st->message, message);\n\t\tst->message = message;\n\t}\n\n\trc = call_status(st, message);')
    return {'baseline': source, 'original-diagnostics': changed}


if __name__ == '__main__':
    driver.REV = 'b856fde4'
    driver.SOURCE_PATHS = ('src/call/call.c',)
    driver.OUT_NAME = 'recovery-call-diagnostics'
    driver.DUMP_FLAGS = ('-v', '-save-temps', '-da')
    driver.variants = variants
    driver.main()
