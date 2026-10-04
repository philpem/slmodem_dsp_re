#!/usr/bin/env python3
"""Replay the sixteen declared V34 rollback source-boundary controls."""
import itertools
import sys
from pathlib import Path
import playbook_small_patterns as driver


def variants(path,source):
    start,end,fn=driver.function(source,'V34EchoHistoryBackwardClean')
    cond='\tif (n > span) {'
    calls='\t\techo_rewind(&obj->echo0, (int)(n - span));\n\t\techo_rewind(&obj->echo1, (int)(n - span));'
    block='''\t} else if ((int)n > 0) {
\t\tshort *p = &pf->state[pf->hist_pos];
\t\tshort *end = &pf->state[pf->hist_len];
\t\tunsigned k = n;

\t\tfor (;;) {
\t\t\t*p++ = 0;
\t\t\tif (p == end)
\t\t\t\tp = pf->state;
\t\t\tif (--k == 0)
\t\t\t\tbreak;
\t\t}
\t}'''
    scope='''\t} else {
\t\tshort *p = &pf->state[pf->hist_pos];
\t\tshort *end = &pf->state[pf->hist_len];
\t\tunsigned k = n;

\t\tif ((int)n > 0) {
\t\t\tfor (;;) {
\t\t\t\t*p++ = 0;
\t\t\t\tif (p == end)
\t\t\t\t\tp = pf->state;
\t\t\t\tif (--k == 0)
\t\t\t\t\tbreak;
\t\t\t}
\t\t}
\t}'''
    assert all(fn.count(v)==1 for v in (cond,calls,block))
    hs,he,helper=driver.function(source,'echo_rewind')
    counter=helper.replace('\tint k;\n','').replace('\tfor (k = 0; k < n; k++) {','\twhile (n > 0) {').replace('\n\t}\n}', '\n\t\tn--;\n\t}\n}')
    assert counter!=helper and counter.count('\t\tn--;')==1
    forms={}
    for signed,cached,early,countdown in itertools.product((False,True),repeat=4):
        text=fn
        if signed:text=text.replace(cond,'\tif ((int)n > (int)span) {')
        if cached:text=text.replace(calls,'\t\tint remaining = (int)(n - span);\n\n\t\techo_rewind(&obj->echo0, remaining);\n\t\techo_rewind(&obj->echo1, remaining);')
        if early:text=text.replace(block,scope)
        whole=source[:start]+text+source[end:]
        if countdown:
            a,z,_=driver.function(whole,'echo_rewind')
            whole=whole[:a]+counter+whole[z:]
        label='-'.join(n for n,v in [('signed',signed),('cached',cached),('early-cursor',early),('countdown',countdown)] if v) or 'baseline'
        forms[label]=whole
    assert len(forms)==len(set(forms.values()))==16
    return forms


if __name__=='__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
    driver.REV='5fac8659'
    driver.OUT_NAME='gcc3-v34-rewind-boundaries'
    driver.SOURCE_PATHS=('src/pump/v34/v34filters.c',)
    driver.variants=variants
    driver.main()
