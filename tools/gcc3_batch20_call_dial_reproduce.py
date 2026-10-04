#!/usr/bin/env python3
"""Cross selected dial-string ownership with observed unsigned S-register tests."""
import itertools
import sys
from pathlib import Path
import playbook_small_patterns as driver
from gcc3_batch20_call_processing_reproduce import variants as preceding

def variants(path,source):
    fixed=preceding(path,source)['create-debug']
    result={'baseline':source}
    for direct,unsigned in itertools.product((False,True),repeat=2):
        text=fixed
        if direct:
            start=text.index('static const char *\ncall_dial_string(');end=text.index('\n}\n',start)+3
            text=text[:start]+text[end:]
            text=text.replace('\tint mode;','\tint mode;\n\tconst char *want;')
            old='''\tCALLPROG_Dial(&st->callprog, call_dial_string(st, dialstr,
\t\t\t\t\t\t      sizeof(dialstr)));'''
            assert text.count(old)==1
            text=text.replace(old,'''\twant = (const char *)(intptr_t)modem_get_param(st->modem, MDMPRM_DIALSTR);
\tif ((unsigned char)(*want - '0') <= 9 && sysdep_strlen(want) <= 62) {
\t\tdialstr[0] = modem_get_sreg(st->modem, 16) < 1 ? 'p' : 't';
\t\tsysdep_strcpy(dialstr + 1, want);
\t\twant = dialstr;
\t}
\tCALLPROG_Dial(&st->callprog, want);''')
        if unsigned:
            text=text.replace('cfg.w0 = (mode <= 1 || mode == 3)', 'cfg.w0 = ((unsigned)mode <= 1 || mode == 3)')
            text=text.replace('modem_get_sreg(st->modem, 16) < 1', '(unsigned long)modem_get_sreg(st->modem, 16) < 1')
        name='-'.join(x for x,v in [('direct-owner',direct),('unsigned-tests',unsigned)] if v) or 'create-debug'
        result[name]=text
    assert len(result)==len(set(result.values()))==5
    return result
if __name__=='__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
    driver.REV='93d7eee1';driver.SOURCE_PATHS=('src/call/call.c',)
    driver.OUT_NAME='gcc3-batch20-call-dial';driver.variants=variants;driver.main()
