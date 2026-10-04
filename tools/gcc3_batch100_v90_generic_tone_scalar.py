#!/usr/bin/env python3
import itertools
import playbook_small_patterns as d
d.REV='856c1ecb';d.SOURCE_PATHS=('src/dsp/GenericToneDetector.cpp',);d.OUT_NAME='gcc3-batch100-v90-generic-tone-scalar'
def variants(path,source):
 cells={}
 for stores,guard in itertools.product((False,True),repeat=2):
  start=source.index('int GenericToneDetector::process(float sample)');end=source.index('\n}\n',start)+2;fn=source[start:end]
  if stores:fn=fn.replace('\t\tacc_0c = in;\n\t\tacc_10 = out;','\t\tacc_10 = out;\n\t\tacc_0c = in;')
  if guard:
   faststart=fn.index('\tif (++sampleCount != blockLen) {');fastend=fn.index('\n\t{',faststart)
   fast=fn[faststart:fastend];storesblock=fast[fast.index('\n\t\tacc_'):fast.index('\n\t\treturn')]
   fn=fn[:faststart]+'\tif (++sampleCount == blockLen) '+fn[fastend:].lstrip('\n\t')
   fn=fn.replace('\n\t}\n\n\treturn (int)detected;','\n\t} else {'+storesblock+'\n\t}\n\n\treturn (int)detected;')
  label='-'.join(n for n,v in [('output-first-store',stores),('positive-block-guard',guard)] if v) or 'baseline';cells[label]=source[:start]+fn+source[end:]
 return cells
d.variants=variants
if __name__=='__main__':d.main()
