#!/usr/bin/env python3
"""Cross five observed owned class-pointer lifetimes in V92Modulator."""
import sys,subprocess
import playbook_small_patterns as driver

OWNERS=(('phase3Modulator','V92Phase3Modulator'),('phase4Modulator','V92Phase4Modulator'),('bitsToSymbol','V92BitsToSymbol'),('queue','Queue'),('txFilter','FloatFIR'))

def variants(path,source):
    marker='#include "dsplib/sysdep.h"'
    assert source.count(marker)==1
    adapter=source.replace(marker,marker+'\n\ninline void operator delete(void *p) { sysdep_free(p); }')
    cells={'baseline':source,'adapter-only':adapter}
    for mask in range(1,32):
        text=adapter
        for i,(member,cls) in enumerate(OWNERS):
            if not mask & (1<<i):continue
            old='\tif ('+member+' != 0) {\n\t\t'+member+'->~'+cls+'();\n\t\tsysdep_free('+member+');\n\t}'
            assert text.count(old)==1,(member,text.count(old))
            text=text.replace(old,'\tdelete '+member+';')
        cells['owners-'+format(mask,'05b')]=text
    return cells

if __name__=='__main__':
    assert '--domain' in sys.argv
    assert sys.argv[sys.argv.index('--domain')+1].startswith('https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-')
    driver.REV='7b201c02'
    driver.OUT_NAME='playbook-v92-owned-delete'
    driver.SOURCE_PATHS=('src/pump/v90/V92Modulator.cpp',)
    driver.variants=variants
    for path in driver.SOURCE_PATHS:
        variants(path,subprocess.check_output(['git','show',driver.REV+':'+path],cwd=driver.ROOT,text=True))
    driver.main()
