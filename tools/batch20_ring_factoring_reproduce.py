#!/usr/bin/env python3
"""Bounded source call-versus-duplicate body for original ring constructor."""
import playbook_small_patterns as d
d.REV='93d7eee1';d.SOURCE_PATHS=('src/service/ringDetector.c',);d.OUT_NAME='batch20-ring-factoring'
def variants(path,source):
 a,b,fn=d.function(source,'RingDetector_Reset');body=fn.split('{',1)[1].rsplit('}',1)[0]
 a,b,create=d.function(source,'RingDetector_Create');assert create.count('\tRingDetector_Reset(s, c);')==1
 create=create.replace('\tRingDetector_Reset(s, c);','\t{'+body+'\t}')
 return {'baseline':source,'body':source[:a]+create+source[b:]}
d.variants=variants
if __name__=='__main__':d.main()
