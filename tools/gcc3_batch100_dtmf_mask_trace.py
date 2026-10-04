#!/usr/bin/env python3
"""Report eager/short-circuit/conditional-clear lowering with stated cases."""
import json,re
from pathlib import Path
import playbook_small_patterns as d
reports={}
for family,label in [('dtmf-eager','baseline'),('dtmf-eager','eager-1-early-1'),('dtmf-mask','clear-early-1')]:
 root=d.ROOT/('build/gcc3-batch100-'+family)/'Dtmf'/label
 stages={}
 for stage in ('01.rtl','14.ce1','21.ce2','29.ce3'):
  text=(root/('Dtmf.c.'+stage)).read_text();text=text.split(';; Function dtmf_test',1)[1].split(';; Function ',1)[0]
  stages[stage]={'and_si':len(re.findall(r'\(and:SI',text)),'neg_si':len(re.findall(r'\(neg:SI',text)),'if_then_else':text.count('(if_then_else'),'lines':len(text.splitlines())}
 reports[family+'/'+label]=stages
# Known source family must cause an actual diagnostic difference.
assert reports['dtmf-eager/baseline']['01.rtl']!=reports['dtmf-eager/eager-1-early-1']['01.rtl']
assert reports['dtmf-mask/clear-early-1']['14.ce1']['and_si']>0
out=d.ROOT/'build/batch100-dtmf-mask-trace.json';out.write_text(json.dumps({'cases':len(reports),'stages_per_case':4,'reports':reports},indent=2)+'\n')
print(json.dumps({'cases':len(reports),'stages_per_case':4,'reports':reports},indent=2))
