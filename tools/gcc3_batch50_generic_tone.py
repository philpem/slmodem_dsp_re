#!/usr/bin/env python3
"""Cross original block fast-arm store order and count ownership."""
import playbook_small_patterns as d
def variants(path,source):
 a=source.index('int GenericToneDetector::process(float *samples, unsigned int n)')
 z=source.index('\n}',a)+2;fn=source[a:z];cells={}
 for stores in (0,1):
  for count in (0,1):
   q=fn
   if stores:
    old='\t\t\tacc_0c = in;\n\t\t\tacc_10 = out;'
    assert q.count(old)==1
    q=q.replace(old,'\t\t\tacc_10 = out;\n\t\t\tacc_0c = in;')
   if count:q=q.replace('\tunsigned int k;\n\n','').replace('for (k = 0; k < n; k++)','for (; n > 0; n--)')
   label='baseline' if not(stores or count) else f'out-first-{stores}-countdown-{count}'
   cells[label]=source[:a]+q+source[z:]
 assert len(set(cells.values()))==4
 return cells
if __name__=='__main__':
 d.REV='902f47fa';d.SOURCE_PATHS=('src/dsp/GenericToneDetector.cpp',);d.OUT_NAME='gcc3-batch50-generic-tone';d.variants=variants;d.main()
