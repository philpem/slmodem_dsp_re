#!/usr/bin/env python3
"""Cross original shared strong/weak HIT updates with known store/count axes."""
import playbook_small_patterns as d
from gcc3_batch50_generic_tone import variants as prior
def variants(path,source):
 cells={}
 for label,text in prior(path,source).items():
  for hit in (0,1):
   q=text
   if hit:
    old='\t\t\t\tcount_30 = 0;\n\t\t\t\tcount_2c++;'
    assert q.count(old)==1
    q=q.replace(old,'\t\t\t\tcount_2c++;\n\t\t\t\tcount_30 = 0;')
   cells[label if not hit else label+'-shared-hit']=q
 assert len(set(cells.values()))==8
 return cells
if __name__=='__main__':
 d.REV='902f47fa';d.SOURCE_PATHS=('src/dsp/GenericToneDetector.cpp',);d.OUT_NAME='gcc3-batch50-generic-tone-hits';d.variants=variants;d.main()
