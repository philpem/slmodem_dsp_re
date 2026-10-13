#!/usr/bin/env python3
"""Recover an independently witnessed V23 create diagnostic boundary."""
import playbook_small_patterns as d


def variants(path, source):
    marker = '\t/* The original prints "v23: create...\\n" here at debug level 2. */'
    assert source.count(marker) == 1
    changed = source.replace('#include "dsplib/dp_wrapper.h"', '#include "dsplib/debug.h"\n#include "dsplib/dp_wrapper.h"')
    changed = changed.replace(marker, '\tif (DSPLIB_DEBUG_ON())\n\t\tdsplibs_debug_printf("v23: create...\\n");')
    status_marker = '\t/* The original prints "v23: V23STAT: --> %d\\n" on a change. */'
    assert source.count(status_marker) == 1
    def process(text):
        declaration = '\tint status;'
        assert text.count(declaration) == 1
        text = text.replace(declaration, '\tint status;\n\tint modem_status;')
        needle = '\tif (self->connected != 0 && n_rx > 0) {'
        assert text.count(needle) == 1
        text = text.replace(needle, '\tmodem_status = status;\n\n'+needle)
        return text.replace(status_marker, '\tif (dp->status != (unsigned)status && DSPLIB_DEBUG_ON())\n\t\tdsplibs_debug_printf("v23: V23STAT: --> %d\\n", modem_status);')
    process_only = process(source.replace('#include "dsplib/dp_wrapper.h"', '#include "dsplib/debug.h"\n#include "dsplib/dp_wrapper.h"'))
    both = process(changed)
    assignments = '\tdp->dp.id = id;\n\tdp->dp.modem = modem;'
    assert both.count(assignments) == 1
    reordered = both.replace(assignments, '\tdp->dp.modem = modem;\n\tdp->dp.id = id;')
    return {'baseline':source,'create-debug':changed,'process-debug':process_only,
            'both-debug':both,'both-debug-owner-order':reordered}


if __name__ == '__main__':
    d.REV = '861901c2'
    d.SOURCE_PATHS = ('src/pump/v23/v23.c',)
    d.OUT_NAME = 'gcc3-missing-debug-boundary'
    d.DUMP_FLAGS = ('-v','-save-temps','-da')
    d.variants = variants
    d.main()
