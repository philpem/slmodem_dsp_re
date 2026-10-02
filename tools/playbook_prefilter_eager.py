#!/usr/bin/env python3
"""Compare logical/eager Boolean OR against the observed capability predicate."""
import playbook_small_patterns as driver

def variants(path,source):
 start,end,fn=driver.function(source,'V90PreFilter::isV90WithEia6')
 old='(cap == 1) || (V90PW(params)[0x500 / 4] == 6)'
 assert fn.count(old)==1
 candidate=fn.replace(old,'(cap == 1) | (V90PW(params)[0x500 / 4] == 6)')
 return {'baseline':source,'eager-or':source[:start]+candidate+source[end:]}

if __name__=='__main__':
 driver.REV='0a9b564f'
 driver.OUT_NAME='playbook-prefilter-eager'
 driver.SOURCE_PATHS=('src/pump/v90/V90PreFilter.cpp',)
 driver.variants=variants
 driver.main()
