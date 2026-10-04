#!/usr/bin/env python3
"""Replay original V23 diagnostic sites established by blob relocations."""
import playbook_small_patterns as d
d.REV='93d7eee1'
d.SOURCE_PATHS=('src/pump/v23/v23rx.c','src/pump/v23/v23.c')
d.OUT_NAME='batch20-v23-debug'
def variants(path,source):
    new=source.replace('#include "dsplib/sysdep.h"','#include "dsplib/sysdep.h"\n#include "dsplib/debug.h"')
    if path.endswith('/v23rx.c'):
        old='''\t/*
\t * The original prints "V23FP Rx Created, version 10-December-02." at
\t * debug level 2.  Dropped, as everywhere else in this tree.
\t */'''
        replacement='\tif (DSPLIB_DEBUG_ON())\n\t\tdsplibs_debug_printf("V23FP Rx Created, version 10-December-02.\\r\\n");'
    else:
        old='\t/* The original prints "v23: create...\\n" here at debug level 2. */'
        replacement='\tif (DSPLIB_DEBUG_ON())\n\t\tdsplibs_debug_printf("v23: create...\\n");'
    assert new.count(old)==1
    new=new.replace(old,replacement)
    return {'baseline':source,'debug':new}
d.variants=variants
if __name__=='__main__':d.main()
