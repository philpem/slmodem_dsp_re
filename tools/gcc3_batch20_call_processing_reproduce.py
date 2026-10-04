#!/usr/bin/env python3
"""Restore original call diagnostics and message/owner handoff boundaries."""
import itertools
import sys
from pathlib import Path
import playbook_small_patterns as driver
from gcc3_batch20_call_delete_reproduce import variants as deletion

def variants(path,source):
    fixed=deletion(path,source)['restore-entry-debug']
    result={'baseline':source}
    for create,message,owner in itertools.product((False,True),repeat=3):
        text=fixed
        if create:
            needle='\tst = sysdep_malloc(sizeof(struct call_dp));'
            assert text.count(needle)==1
            text=text.replace(needle,'\tif (DSPLIB_DEBUG_ON())\n\t\tdsplibs_debug_printf("call: create...\\n");\n\n'+needle)
            needle='\tif (srate != CALL_NATIVE_RATE) {'
            assert text.count(needle)==1
            text=text.replace(needle,needle+'\n\t\tif (DSPLIB_DEBUG_ON())\n\t\t\tdsplibs_debug_printf("call: create RC: %d <-> %d...\\n",\n\t\t\t\t\t     srate, CALL_NATIVE_RATE);')
        if message:
            start=text.index('static int\ncall_block(');end=text.index('\n}\n',start)+3;fn=text[start:end]
            assert fn.count('\tint rc;')==1
            fn=fn.replace('\tint rc;','\tint rc;\n\tint message;')
            fn=fn.replace('st->message = CALLPROG_Progress','message = CALLPROG_Progress')
            needle='\trc = call_status(st, st->message);'
            assert fn.count(needle)==1
            fn=fn.replace(needle,'''\tif (message != st->message) {
\t\tif (DSPLIB_DEBUG_ON())
\t\t\tdsplibs_debug_printf("call: process: msg %d --> %d\\n",
\t\t\t\t\t     st->message, message);
\t\tst->message = message;
\t}
\trc = call_status(st, message);''')
            text=text[:start]+fn+text[end:]
        if owner:
            old='''call_block(struct call_dp *st)
{
\tshort *from_line = st->in_q.data + st->in_q.active / 2;
\tshort *to_line = st->out_q.data + st->out_q.active / 2;'''
            new='''call_block(struct dp *dp, short *from_line, short *to_line)
{
\tstruct call_dp *st = ((struct call_dp *)dp)->self;'''
            assert text.count(old)==1 and text.count('call_block(st)')==1
            text=text.replace(old,new).replace('call_block(st)','call_block(dp,\n\t\t\t\tst->in_q.data + st->in_q.active / 2,\n\t\t\t\tst->out_q.data + st->out_q.active / 2)')
        name='-'.join(x for x,v in [('create-debug',create),('message-boundary',message),('owner-reload',owner)] if v) or 'delete-debug'
        result[name]=text
    assert len(result)==len(set(result.values()))==9
    return result
if __name__=='__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
    driver.REV='93d7eee1'
    driver.SOURCE_PATHS=('src/call/call.c',)
    driver.OUT_NAME='gcc3-batch20-call-processing'
    driver.variants=variants
    driver.main()
