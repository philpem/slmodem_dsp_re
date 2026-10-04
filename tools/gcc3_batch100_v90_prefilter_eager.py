#!/usr/bin/env python3
import playbook_small_patterns as d
d.REV='856c1ecb';d.SOURCE_PATHS=('src/pump/v90/V90PreFilter.cpp',);d.OUT_NAME='gcc3-batch100-v90-prefilter-eager'
def variants(path,source):
 old='return (cap == 1) || (V90PW(params)[0x500 / 4] == 6);';new=old.replace(' || ',' | ')
 assert source.count(old)==1
 return {'baseline':source,'eager-equalities':source.replace(old,new)}
d.variants=variants
if __name__=='__main__':d.main()
