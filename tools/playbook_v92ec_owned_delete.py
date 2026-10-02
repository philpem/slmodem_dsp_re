#!/usr/bin/env python3
"""Test the captured FloatARMA owner with an independent delete adapter."""
import sys, subprocess
import playbook_small_patterns as driver

def variants(path, source):
    marker='#include "dsplib/vpcm_tables.h"'
    assert source.count(marker)==1
    adapter=source.replace(marker,marker+'\n\ninline void operator delete(void *p) { sysdep_free(p); }')
    old='\t\tarma->~FloatARMA();\n\t\tsysdep_free(arma);'
    assert adapter.count(old)==1
    return {'baseline':source,'adapter-only':adapter,'delete':adapter.replace(old,'\t\tdelete arma;')}

if __name__=='__main__':
    assert '--domain' in sys.argv
    assert sys.argv[sys.argv.index('--domain')+1].startswith('https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-')
    driver.REV='046d129d'
    driver.OUT_NAME='playbook-v92ec-owned-delete'
    driver.SOURCE_PATHS=('src/pump/v90/V92EchoCanceller.cpp',)
    driver.variants=variants
    for path in driver.SOURCE_PATHS:
        variants(path,subprocess.check_output(['git','show',driver.REV+':'+path],cwd=driver.ROOT,text=True))
    driver.main()
