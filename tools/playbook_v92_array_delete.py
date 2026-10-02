#!/usr/bin/env python3
"""Array deletion after independently recovered V92 C++ TU provenance."""
import sys, subprocess, re
import playbook_small_patterns as driver

def variants(path,source):
    cells={'baseline':source}
    for label,constellations,filters in [('constellations',True,False),('filters',False,True),('both',True,True)]:
        text=source
        marker='#include "dsplib/sysdep.h"'
        assert text.count(marker)==1
        text=text.replace(marker,marker+'\n\ninline void operator delete[](void *p) { sysdep_free(p); }')
        for name,active in [('V92deleteConstellations',constellations),('V92deleteFilterCoefficients',filters)]:
            if not active:
                continue
            start,end,fn=driver.function(text,name)
            fn,n=re.subn(r'\tif \((p->[^\n]+) != 0\)\n\t\tsysdep_free\(\1\);',r'\tdelete[] \1;',fn)
            assert n==(6 if name=='V92deleteConstellations' else 4),(name,n)
            text=text[:start]+fn+text[end:]
        cells[label]=text
    return cells

if __name__=='__main__':
    assert '--domain' in sys.argv
    assert sys.argv[sys.argv.index('--domain')+1].startswith('https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-')
    driver.REV='ef019461'
    driver.OUT_NAME='playbook-v92-array-delete'
    driver.SOURCE_PATHS=('src/pump/v90/V92MappingParamsInt.cpp',)
    driver.variants=variants
    for p in driver.SOURCE_PATHS:
        variants(p,subprocess.check_output(['git','show',driver.REV+':'+p],cwd=driver.ROOT,text=True))
    driver.main()
