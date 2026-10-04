#!/usr/bin/env python3
import re,sys
from pathlib import Path
import playbook_small_patterns as d
import gcc3_batch100_vtb_absolute_reproduce as absolute

def variants(path,source):
 controlled=absolute.variants(path,source);cells={'baseline':source}
 for owners in ('retained','owners'):
  for minimum in (0,1):
   label=owners+'-absolute-affine-minimum-'+str(minimum);s=controlled[label]
   for literal in (0,1):
    t=s
    if literal:
     a,z,helper=d.function(t,'vtb_acs');body=helper[helper.index('{'):]
     a,z,main=d.function(t,'VTB_decoder')
     pattern=r'\tvtb_acs\(state, node, ([0-7]), ([04]), ([0-3]), old, bm, pt\);'
     def expand(match):
      constants=dict(zip(('s','p0','x'),match.groups()))
      block=re.sub(r'\b(?:s|p0|x)\b',lambda m:constants[m.group()],body)
      return '\t'+block.replace('\n','\n\t').rstrip('\t')
     main,count=re.subn(pattern,expand,main);assert count==8
     t=t[:a]+main+t[z:]
    cells[label+'-literal-'+str(literal)]=t
 assert len(set(cells.values()))==9
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='856c1ecb';d.OUT_NAME='gcc3-batch100-vtb-literal';d.SOURCE_PATHS=('src/dsp/fpm_vtb.c',);d.variants=variants;absolute.allow_recorded_static_helper();d.main()
