#!/usr/bin/env python3
"""Cross independently captured owned class pointers in the two modem TUs."""
import sys, subprocess
import playbook_small_patterns as driver

FAMILIES={
 'V90Modem': ('#include "dsplib/V92Jd.h"',(('modulator','V90Modulator'),('demodulator','V90Demodulator'),('jd','V90Jd'),('jd92','V92Jd'),('params','V90Parameters'))),
 'V92Modem': ('#include "dsplib/V92Phase2Info.h"',(('modulator','V92Modulator'),('cp','V92CP'),('parameters','V92Parameters')))}

def variants(path,source):
    family=path.rsplit('/',1)[1].split('.')[0]
    marker,owners=FAMILIES[family]
    assert source.count(marker)==1
    adapter=source.replace(marker,marker+'\n\ninline void operator delete(void *p) { sysdep_free(p); }')
    cells={'baseline':source,'adapter-only':adapter}
    for mask in range(1,1<<len(owners)):
        text=adapter
        for i,(member,cls) in enumerate(owners):
            if not mask & (1<<i):continue
            old='\tif ('+member+' != 0) {\n\t\t'+member+'->~'+cls+'();\n\t\tsysdep_free('+member+');\n\t}'
            assert text.count(old)==1,member
            text=text.replace(old,'\tdelete '+member+';')
        cells['owners-'+format(mask,'0'+str(len(owners))+'b')]=text
    return cells

if __name__=='__main__':
    assert '--domain' in sys.argv
    assert sys.argv[sys.argv.index('--domain')+1].startswith('https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-')
    driver.REV='a3da40f7'
    driver.OUT_NAME='playbook-modem-owned-delete'
    driver.SOURCE_PATHS=tuple('src/pump/v90/'+n+'.cpp' for n in FAMILIES)
    driver.variants=variants
    for path in driver.SOURCE_PATHS:
        variants(path,subprocess.check_output(['git','show',driver.REV+':'+path],cwd=driver.ROOT,text=True))
    driver.main()
