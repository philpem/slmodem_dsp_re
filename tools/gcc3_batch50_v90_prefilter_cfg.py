#!/usr/bin/env python3
import itertools
import playbook_small_patterns as d
d.REV='902f47fa'
d.SOURCE_PATHS=('src/pump/v90/V92PreFilter.cpp',)
d.OUT_NAME='gcc3-batch50-v90-prefilter-cfg'
def variants(path,source):
 start,end,fn=d.function(source,'V92PreFilter::process');cells={}
 old='\t\tif (tapsIir != 0) {\n\t\t\tfir->process(in, tmp, V92PREFILTER_SAMPLES);\n\t\t\tiir->process(tmp, out, V92PREFILTER_SAMPLES);\n\t\t} else {\n\t\t\tfir->process(in, out, V92PREFILTER_SAMPLES);\n\t\t}'
 new='\t\tif (tapsIir == 0) {\n\t\t\tfir->process(in, out, V92PREFILTER_SAMPLES);\n\t\t} else {\n\t\t\tfir->process(in, tmp, V92PREFILTER_SAMPLES);\n\t\t\tiir->process(tmp, out, V92PREFILTER_SAMPLES);\n\t\t}'
 assert fn.count(old)==1
 for hot,inclusive in itertools.product((False,True),repeat=2):
  body=fn.replace(old,new) if hot else fn
  if inclusive:body=body.replace('i < V92PREFILTER_SAMPLES','i <= V92PREFILTER_SAMPLES - 1')
  label='-'.join(n for n,v in [('fir-hot',hot),('inclusive-copy',inclusive)] if v) or 'baseline';cells[label]=source[:start]+body+source[end:]
 for label,hot in [('do-copy',False),('fir-hot-do-copy',True)]:
  body=fn.replace(old,new) if hot else fn
  oldloop='\t\tfor (i = 0; i < V92PREFILTER_SAMPLES; i++)\n\t\t\tout[i] = in[i];'
  assert body.count(oldloop)==1
  body=body.replace(oldloop,'\t\ti = 0;\n\t\tdo {\n\t\t\tout[i] = in[i];\n\t\t} while (++i <= V92PREFILTER_SAMPLES - 1);')
  cells[label]=source[:start]+body+source[end:]
 return cells
d.variants=variants
if __name__=='__main__':d.main()
