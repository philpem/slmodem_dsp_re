#!/usr/bin/env python3
"""Known-source mode/CFG controls, plus exhaustive32-cell VTB subset-map proof."""
import json
import playbook_small_patterns as d
cases=[('fsd-mask','fpm_fsd','baseline','FPM_FSD_demodulate'),('fsd-mask','fpm_fsd','cfg-1-state-1-early-1-clear-1','FPM_FSD_demodulate'),('vtb-snapshot','fpm_vtb','baseline','VTB_decoder'),('vtb-snapshot','fpm_vtb','word-0-cursor-0-abs-1','VTB_decoder'),('vtb-absolute','fpm_vtb','owners-absolute-affine-minimum-1','VTB_decoder'),('vtb-literal','fpm_vtb','owners-absolute-affine-minimum-1-literal-1','VTB_decoder')]
reports={}
for family,tu,label,fn in cases:
 root=d.ROOT/('build/gcc3-batch100-'+family)/tu/label;stages={}
 for stage in ('01.rtl','14.ce1','21.ce2','29.ce3'):
  text=(root/(tu+'.c.'+stage)).read_text().split(';; Function '+fn,1)[1].split(';; Function ',1)[0]
  stages[stage]={name:text.count('('+name) for name in ('if_then_else','and:SI','neg:SI','xor:SI','ashiftrt:SI')}
 reports[family+'/'+label]={'stages':stages,'emitted_functions':sorted(d.b.sizes(root/'candidate.o'))}
original=reports['vtb-snapshot/baseline']['stages']['01.rtl'];absolute=reports['vtb-snapshot/word-0-cursor-0-abs-1']['stages']['01.rtl']
assert absolute['if_then_else']==original['if_then_else']-2
assert absolute['xor:SI']==original['xor:SI']+2
assert 'vtb_acs' in reports['vtb-absolute/owners-absolute-affine-minimum-1']['emitted_functions']
assert 'vtb_acs' not in reports['vtb-literal/owners-absolute-affine-minimum-1-literal-1']['emitted_functions']
map_checks=0
for p0 in (0,4):
 for x in range(4):
  for relative in range(4):
   k=p0+relative
   if p0==0:affine=(k,k^1,k+2,3-k)[x]
   else:affine=(k,9-k,k-2,11-k)[x]
   assert 0<=affine<8 and affine%4==(relative^x)
   map_checks+=1
result={'source_cases':len(cases),'stages_per_case':4,'subset_map_checks':map_checks,'reports':reports}
(d.ROOT/'build/batch100-numeric-boundary-trace.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({'source_cases':len(cases),'stage_cases':len(cases)*4,'subset_map_checks':map_checks,'known_controls_fire':True}))
