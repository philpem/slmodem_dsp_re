#!/usr/bin/env python3
"""Test the scalar value boundary accepted by GCC's eager truth folding."""
import playbook_small_patterns as driver

def variants(path,source):
 start,end,fn=driver.function(source,'V90PreFilter::isV90WithEia6')
 old='return (cap == 1) || (V90PW(params)[0x500 / 4] == 6);'
 assert fn.count(old)==1
 candidate=fn.replace(old,'int registryLoopType = V90PW(params)[0x500 / 4];\n\treturn (cap == 1) || (registryLoopType == 6);')
 return {'baseline':source,'value-capture':source[:start]+candidate+source[end:]}

if __name__=='__main__':
 driver.REV='0c5d71b4'
 driver.OUT_NAME='playbook-prefilter-value'
 driver.SOURCE_PATHS=('src/pump/v90/V90PreFilter.cpp',)
 driver.variants=variants
 driver.main()
