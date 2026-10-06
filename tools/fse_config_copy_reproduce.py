#!/usr/bin/env python3
"""Cross original configuration-copy form and reset-store boundary."""
import playbook_small_patterns as d


def variants(path, source):
    copy = '\tstate->cfg = *cfg;\n'
    boundary = '\tstate->lms_on = 1;\n'
    assert source.count(copy) == source.count(boundary) == 1
    result = {'baseline': source}
    for explicit in (False, True):
        for delayed in (False, True):
            if not explicit and not delayed:
                continue
            candidate = source
            spelling = copy
            if explicit:
                candidate = candidate.replace('#include "dsplib/debug.h"', '#include <string.h>\n\n#include "dsplib/debug.h"')
                spelling = '\tmemcpy(&state->cfg, cfg, sizeof(state->cfg));\n'
            if delayed:
                candidate = candidate.replace(copy, '').replace(boundary, boundary + spelling)
            else:
                candidate = candidate.replace(copy, spelling)
            result['memcpy-%d-delayed-%d' % (explicit, delayed)] = candidate
    return result


if __name__ == '__main__':
    d.REV = 'e0052eec'
    d.SOURCE_PATHS = ('src/dsp/fpm_fse.c',)
    d.OUT_NAME = 'fse-config-copy'
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dP')
    d.variants = variants
    d.main()
