#!/usr/bin/env python3
"""Separate a scalar delete adapter from the owned converter expression."""
import sys,subprocess
import playbook_small_patterns as driver

def variants(path,source):
    marker='#include "dsplib/V90Phase4Modulator.h"'
    assert source.count(marker)==1
    adapter=source.replace(marker,marker+'\n\ninline void operator delete(void *p) { sysdep_free(p); }')
    old='\tif (!externalBitsToSymbol && bitsToSymbol) {\n\t\tbitsToSymbol->~V90BitsToSymbol();\n\t\tsysdep_free(bitsToSymbol);\n\t}'
    assert adapter.count(old)==1
    text=adapter.replace(old,'\tif (!externalBitsToSymbol)\n\t\tdelete bitsToSymbol;')
    return {'baseline':source,'adapter-only':adapter,'delete':text}

if __name__=='__main__':
    assert '--domain' in sys.argv
    assert sys.argv[sys.argv.index('--domain')+1].startswith('https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-')
    driver.REV='6d8e4a1c'
    driver.OUT_NAME='playbook-v90p4-owned-delete'
    driver.SOURCE_PATHS=('src/pump/v90/V90Phase4Modulator.cpp',)
    driver.variants=variants
    for path in driver.SOURCE_PATHS:
        variants(path,subprocess.check_output(['git','show',driver.REV+':'+path],cwd=driver.ROOT,text=True))
    driver.main()
