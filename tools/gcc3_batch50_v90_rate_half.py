#!/usr/bin/env python3
import itertools
import playbook_small_patterns as d
d.REV='902f47fa';d.SOURCE_PATHS=('src/pump/v90/V90Modulator.cpp',);d.OUT_NAME='gcc3-batch50-v90-rate-half'
def variants(path,source):
 cells={}
 for entry,progress in itertools.product((False,True),repeat=2):
  text=source
  for method,enabled in [('enterDataPhase',entry),('progress',progress)]:
   if enabled:
    start,end,fn=d.function(text,'V90Modulator::'+method)
    assert fn.count('(0.5f +')==1;fn=fn.replace('(0.5f +','(0.5 +');text=text[:start]+fn+text[end:]
  label='-'.join(n for n,v in [('entry-double-half',entry),('progress-double-half',progress)] if v) or 'baseline';cells[label]=text
 return cells
d.variants=variants
if __name__=='__main__':d.main()
