#!/usr/bin/env python3
"""Original immediate-mode creation boundary, distinct direction results."""
import playbook_small_patterns as d
PAIRS=[(8000,9600,2),(9600,8000,3),(8000,48000,4),(48000,8000,5),(9600,48000,6),(48000,9600,7)]
def arms(left,right,out):
 s=''
 for i,(f,t,m) in enumerate(PAIRS):s+=f'\t\t{"if" if not i else "else if"} ({left} == {f} && {right} == {t})\n\t\t\t{out} = RcFixed_Create({m});\n'
 return s
def variants(path,source):
 cells={'baseline':source}
 for direct in (0,1):
  start,end,fn=d.function(source,'dp_wrapper_create')
  a=fn.index('\tif (host_srate != dp_srate) {');z=fn.rindex('\n\treturn w;')
  q='\tif (host_srate != dp_srate) {\n\t\tstruct rc *to_dp = 0;\n\t\tstruct rc *to_host = 0;\n\n'
  for left,right,out,field in [('host_srate','dp_srate','to_dp','rc_to_dp'),('dp_srate','host_srate','to_host','rc_to_host')]:
   q+=arms(left,right,out) if direct else f'\t\t{out} = dpw_create_for({left}, {right});\n'
   q+=f'\t\tw->{field} = {out};\n\t\tif ({out} == 0) {{\n\t\t\tdp_wrapper_delete(w);\n\t\t\treturn NULL;\n\t\t}}\n'
  q+='\t}\n'
  text=source[:start]+fn[:a]+q+fn[z:]+source[end:]
  a=text.index('struct dpw_rate_pair {');z=text.index('\nstruct dp_wrapper *',a)
  helper='' if direct else 'static struct rc *\ndpw_create_for(int from, int to)\n{\n\tstruct rc *result = 0;\n'+arms('from','to','result')+'\treturn result;\n}\n'
  text=text[:a]+helper+text[z:]
  cells['direct-arms' if direct else 'creation-helper']=text
 assert len(set(cells.values()))==3
 return cells
if __name__=='__main__':
 d.REV='902f47fa';d.SOURCE_PATHS=('src/core/dp_wrapper.c',);d.OUT_NAME='gcc3-batch50-wrapper-creation';d.variants=variants;d.main()
