#!/usr/bin/env python3
import itertools
import playbook_small_patterns as d
import gcc3_batch100_v90_generic_tone_scalar as parent
d.OUT_NAME='gcc3-batch100-v90-generic-tone-mean'
def variants(path,source):
 cells={}
 for wide,stores in itertools.product((False,True),repeat=2):
  text=parent.variants(path,source)['output-first-store' if stores else 'baseline']
  start=text.index('int GenericToneDetector::process(float sample)');end=text.index('\n}\n',start)+2;fn=text[start:end]
  if wide:fn=fn.replace('float meanOut = out * inv;','double meanOut = out * inv;')
  label='-'.join(n for n,v in [('double-mean-output',wide),('output-first-store',stores)] if v) or 'baseline';cells[label]=text[:start]+fn+text[end:]
 return cells
d.variants=variants
if __name__=='__main__':d.main()
