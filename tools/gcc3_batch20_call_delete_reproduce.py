#!/usr/bin/env python3
"""Restore the original call deletion diagnostic in two complete-TU controls."""
import sys
from pathlib import Path
import playbook_small_patterns as driver

def variants(path,source):
    start,end,fn=driver.function(source,'call_delete')
    needle='\tCALLPROG_Delete(&st->callprog);'
    assert fn.count(needle)==1
    fn=fn.replace(needle,'\tif (DSPLIB_DEBUG_ON())\n\t\tdsplibs_debug_printf("call: delete...\\n");\n\n'+needle)
    return {'baseline':source,'restore-entry-debug':source[:start]+fn+source[end:]}
if __name__=='__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
    driver.REV='93d7eee1'
    driver.SOURCE_PATHS=('src/call/call.c',)
    driver.OUT_NAME='gcc3-batch20-call-delete'
    driver.variants=variants
    driver.main()
