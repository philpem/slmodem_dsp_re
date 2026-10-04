#!/usr/bin/env python3
"""Known AGC source boundaries measured at expansion/loop/ifconversion."""
import json
import playbook_small_patterns as d
cases=[('agc','baseline'),('agc','cursor-0-word-1-gate-0'),('agc','cursor-1-word-0-gate-0'),('agc','cursor-0-word-0-gate-1'),('agc-boolean','gate-word-eager-inner')]
reports={}
for family,label in cases:
 root=d.ROOT/('build/gcc3-batch100-'+family)/'fpm_agc'/label;stages={}
 for stage in ('01.rtl','09.loop','14.ce1','21.ce2'):
  text=(root/('fpm_agc.c.'+stage)).read_text().split(';; Function FPM_AGC_agc',1)[1].split(';; Function ',1)[0]
  stages[stage]={name:text.count('('+name) for name in ('if_then_else','compare:HI','compare:SI','sign_extend:SI','zero_extend:SI','and:SI','and:QI')}
 reports[family+'/'+label]=stages
assert reports['agc/cursor-0-word-1-gate-0']['01.rtl']['sign_extend:SI']>reports['agc/baseline']['01.rtl']['sign_extend:SI']
assert reports['agc-boolean/gate-word-eager-inner']['01.rtl']['and:SI']>reports['agc/cursor-0-word-0-gate-1']['01.rtl']['and:SI']
result={'source_cases':len(cases),'stage_cases':len(cases)*4,'known_controls_fire':True,'reports':reports}
(d.ROOT/'build/batch100-agc-boundary-trace.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({k:v for k,v in result.items() if k!='reports'}))
