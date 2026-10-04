#!/usr/bin/env python3
"""Cross original queue cursor signedness and byte-offset buffer addresses."""
import itertools
import subprocess
import sys
from pathlib import Path
import playbook_small_patterns as driver
from gcc3_batch20_call_processing_reproduce import variants as preceding

def variants(path,source):
    fixed=preceding(path,source)['create-debug-message-boundary-owner-reload']
    result={'baseline':source}
    for unsigned,byte in itertools.product((False,True),repeat=2):
        text=fixed
        if byte:
            for which in ('in','out'):
                old='st->'+which+'_q.data + st->'+which+'_q.active / 2'
                new='(short *)((char *)st->'+which+'_q.data + st->'+which+'_q.active)'
                assert text.count(old)==1
                text=text.replace(old,new)
        name='-'.join(x for x,v in [('unsigned-cursors',unsigned),('byte-buffers',byte)] if v) or 'restored-boundaries'
        result[name]=text
    return result

def overlay(path,label):
    if 'unsigned-cursors' not in label:return {}
    h=subprocess.check_output(['git','show','93d7eee1:include/dsplib/call.h'],cwd=driver.ROOT,text=True)
    for field in ('head','tail'):
        old='\tint\t'+field+';';assert h.count(old)==1
        h=h.replace(old,'\tunsigned int\t'+field+';')
    return {'dsplib/call.h':h}
if __name__=='__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
    driver.REV='93d7eee1';driver.SOURCE_PATHS=('src/call/call.c',)
    driver.OUT_NAME='gcc3-batch20-call-queue';driver.variants=variants;driver.HEADER_OVERLAYS=overlay;driver.main()
