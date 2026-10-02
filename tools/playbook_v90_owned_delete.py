#!/usr/bin/env python3
"""Cross three observed owned class-pointer lifetimes in V90Modulator."""
import sys,subprocess
import playbook_small_patterns as driver

OWNERS=(('phase3Modulator','V90Phase3Modulator'),('phase4Modulator','V90Phase4Modulator'),('bitsToSymbol','V90BitsToSymbol'))

def variants(path,source):
    marker='#include "dsplib/V90Phase4Modulator.h"'
    assert source.count(marker)==1
    adapter=source.replace(marker,marker+'\n\ninline void operator delete(void *p) { sysdep_free(p); }')
    cells={'baseline':source,'adapter-only':adapter}
    for mask in range(1,8):
        text=adapter
        for i,(member,cls) in enumerate(OWNERS):
            if not mask & (1<<i):continue
            old='\tif ('+member+') {\n\t\t'+member+'->~'+cls+'();\n\t\tsysdep_free('+member+');\n\t}'
            assert text.count(old)==1,(member,text.count(old))
            text=text.replace(old,'\tdelete '+member+';')
        cells['owners-'+format(mask,'03b')]=text
    return cells

if __name__=='__main__':
    assert '--domain' in sys.argv
    assert sys.argv[sys.argv.index('--domain')+1].startswith('https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-')
    driver.REV='9daec211'
    driver.OUT_NAME='playbook-v90-owned-delete'
    driver.SOURCE_PATHS=('src/pump/v90/V90Modulator.cpp',)
    driver.variants=variants
    for path in driver.SOURCE_PATHS:
        variants(path,subprocess.check_output(['git','show',driver.REV+':'+path],cwd=driver.ROOT,text=True))
    driver.main()
