#!/usr/bin/env python3
"""Bounded singleton/complement screen of eight observed owner lifetimes."""
import sys,subprocess
import playbook_small_patterns as driver
OWNERS=(('equalizer','V90Equalizer'),('phase3Demodulator','V90Phase3Demodulator'),('phase4Demodulator','V90Phase4Demodulator'),('demapper','V90Demapper'),('trn2Designer','V90TRN2Designer'),('constellationDesigner','V90ConstellationDesigner'),('autoDigitalImpDetector','V90AutoDigitalImpDetector'),('connectionEvaluator','V90ConnectionEvaluator'))

def variants(path,source):
    marker='#include "dsplib/V90MP.h"'
    assert source.count(marker)==1
    adapter=source.replace(marker,marker+'\n\ninline void operator delete(void *p) { sysdep_free(p); }')
    cells={'baseline':source,'adapter-only':adapter}
    masks=[255]+[1<<i for i in range(8)]+[255^(1<<i) for i in range(8)]
    assert len(masks)==len(set(masks))==17
    for mask in masks:
        text=adapter
        for i,(member,cls) in enumerate(OWNERS):
            if not mask & (1<<i):continue
            old='\tif ('+member+') {\n\t\t'+member+'->~'+cls+'();\n\t\tsysdep_free('+member+');\n\t}'
            assert text.count(old)==1,member
            text=text.replace(old,'\tdelete '+member+';')
        cells['owners-'+format(mask,'08b')]=text
    return cells

if __name__=='__main__':
    assert '--domain' in sys.argv
    assert sys.argv[sys.argv.index('--domain')+1].startswith('https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-')
    driver.REV='35096c95'
    driver.OUT_NAME='playbook-v90dem-owned-delete'
    driver.SOURCE_PATHS=('src/pump/v90/V90Demodulator.cpp',)
    driver.variants=variants
    for path in driver.SOURCE_PATHS:
        variants(path,subprocess.check_output(['git','show',driver.REV+':'+path],cwd=driver.ROOT,text=True))
    driver.main()
