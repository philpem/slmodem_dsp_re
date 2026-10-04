#!/usr/bin/env python3
"""Cross observed S56 width, named frame-object order and getter owner."""
import itertools
import sys
from pathlib import Path
import playbook_small_patterns as driver
from gcc3_batch20_call_processing_reproduce import variants as preceding

def variants(path,source):
    fixed=preceding(path,source)['create-debug']
    result={'baseline':source}
    for width,frame,owner in itertools.product((False,True),repeat=3):
        start,end,fn=driver.function(fixed,'call_create')
        if width:fn=fn.replace('\tint mode;', '\tunsigned int mode;')
        if frame:fn=fn.replace('\tstruct callprog_cfg cfg;\n\tchar dialstr[64];', '\tchar dialstr[64];\n\tstruct callprog_cfg cfg;')
        if owner:fn=fn.replace('mode = modem_get_sreg(modem, 56);','mode = modem_get_sreg(st->modem, 56);')
        name='-'.join(x for x,v in [('unsigned-mode',width),('frame-order',frame),('getter-owner',owner)] if v) or 'create-debug'
        result[name]=fixed[:start]+fn+fixed[end:]
    assert len(result)==len(set(result.values()))==9
    return result
if __name__=='__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
    driver.REV='93d7eee1';driver.SOURCE_PATHS=('src/call/call.c',)
    driver.OUT_NAME='gcc3-batch20-call-create';driver.variants=variants;driver.main()
