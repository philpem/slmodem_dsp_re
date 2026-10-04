#!/usr/bin/env python3
"""Original V23 composite/debug state boundaries."""
import itertools
import playbook_small_patterns as d
import batch20_v23_debug_reproduce as debug
d.REV='93d7eee1';d.SOURCE_PATHS=('src/pump/v23/v23modem.c','src/pump/v23/v23.c');d.OUT_NAME='batch20-v23-composite'
def variants(path,source):
    cells={}
    if path.endswith('/v23.c'):
        old='\t/* The original prints "v23: V23STAT: --> %d\\n" on a change. */\n\tdp->status = (unsigned)status;'
        new='\tif (dp->status != (unsigned)status) {\n\t\tif (DSPLIB_DEBUG_ON())\n\t\t\tdsplibs_debug_printf("v23: V23STAT: --> %d\\n", status);\n\t\tdp->status = (unsigned)status;\n\t}'
        assert source.count(old)==1
        for entry,state in itertools.product((False,True),repeat=2):
            text=debug.variants(path,source)['debug'] if entry else source
            if state:
                text=text.replace(old,new)
                if not entry:text=text.replace('#include "dsplib/sysdep.h"','#include "dsplib/sysdep.h"\n#include "dsplib/debug.h"')
            label='baseline' if not(entry or state) else ('entry' if entry else '')+('-state' if state else '')
            cells[label]=text
        return cells
    start=source.index('\t/*\n\t * The original prints "V23FP version')
    end=source.index('\n\n\tm->elapsed',start)
    banner=source[start:end]
    replacement='\t/* Original build identity, recovered from the diagnostic literals. */\n\tif (DSPLIB_DEBUG_ON())\n\t\tdsplibs_debug_printf("V23FP version %s %s\\n",\n\t\t\t\t     "Sep 22 2005", "15:48:09");'
    start=source.index('\t/*\n\t * The original compares these two')
    end=source.index('\n\n\tswitch (m->state)',start)
    transition=source[start:end]
    transnew='\tif (m->reported != m->state) {\n\t\tif (DSPLIB_DEBUG_ON())\n\t\t\tdsplibs_debug_printf("V23ModemMain: modem state moved from %d to %d\\n",\n\t\t\t\t\t     m->reported, m->state);\n\t\tm->reported = m->state;\n\t}'
    for create,answer,state in itertools.product((False,True),repeat=3):
        text=source
        if create:text=text.replace(banner,replacement)
        if answer:text=text.replace('\tif (cfg->answer_tone) {','\tif (cfg->answer_tone) {\n\t\tif (DSPLIB_DEBUG_ON())\n\t\t\tdsplibs_debug_printf("Generating answer tone.\\n");')
        if state:text=text.replace(transition,transnew)
        if create or answer or state:text=text.replace('#include "dsplib/sysdep.h"','#include "dsplib/sysdep.h"\n#include "dsplib/debug.h"')
        label='baseline' if not(create or answer or state) else ('create' if create else '')+('-answer' if answer else '')+('-state' if state else '')
        cells[label]=text
    assert len(cells)==len(set(cells.values()))==8
    return cells
d.variants=variants
if __name__=='__main__':d.main()
