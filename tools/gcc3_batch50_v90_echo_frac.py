#!/usr/bin/env python3
import playbook_small_patterns as d
d.REV='902f47fa'
d.SOURCE_PATHS=('src/pump/v90/V92EchoCanceller.cpp',)
d.OUT_NAME='gcc3-batch50-v90-echo-frac'
def variants(path,source):
 old='((long double)v - (long double)(int)v)'
 assert source.count(old)==1
 return {'baseline':source,**{label:source.replace(old,new) for label,new in [
 ('reverse-longdouble','((long double)(int)v - (long double)v)'),
 ('forward-float','(v - (int)v)'),
 ('reverse-float','((int)v - v)')]}}
d.variants=variants
if __name__=='__main__':d.main()
