#!/usr/bin/env python3
"""Cross matching array allocations with the supported V92 array deletions."""
import sys, subprocess
import playbook_small_patterns as driver
import playbook_v92_array_delete as deletion

def variants(path,source):
    base=deletion.variants(path,source)['both']
    marker='inline void operator delete[](void *p) { sysdep_free(p); }'
    assert base.count(marker)==1
    cells={'baseline':source}
    for label,cons,filters in [('deletion',False,False),('constellations',True,False),('filters',False,True),('both',True,True)]:
        text=base
        if cons or filters:
            text=text.replace(marker,'inline void *operator new[](size_t size) { return sysdep_malloc(size); }\n'+marker)
        if cons:
            old='(int *)sysdep_malloc(V92_PARAMSINFO_CONSTELLATION_SZ)'
            assert text.count(old)==6
            text=text.replace(old,'new int[V92_PARAMSINFO_CONSTELLATION_SZ / sizeof(int)]')
        if filters:
            old='(float *)sysdep_malloc(V92_PARAMSINFO_FILTERCOEF_SZ)'
            assert text.count(old)==4
            text=text.replace(old,'new float[V92_PARAMSINFO_FILTERCOEF_SZ / sizeof(float)]')
        cells[label]=text
    return cells

if __name__=='__main__':
    assert '--domain' in sys.argv
    assert sys.argv[sys.argv.index('--domain')+1].startswith('https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-')
    driver.REV='ef019461'
    driver.OUT_NAME='playbook-v92-array-allocation'
    driver.SOURCE_PATHS=('src/pump/v90/V92MappingParamsInt.cpp',)
    driver.variants=variants
    for p in driver.SOURCE_PATHS:
        variants(p,subprocess.check_output(['git','show',driver.REV+':'+p],cwd=driver.ROOT,text=True))
    driver.main()
