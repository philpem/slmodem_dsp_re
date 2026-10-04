#!/usr/bin/env python3
"""Original nonzero-arm count capture and shared guarded do latch."""
import playbook_small_patterns as d
from gcc3_batch50_generic_tone import variants as prior
def variants(path,source):
 cells={'baseline':source}
 forms=prior(path,source)
 for stores in (0,1):
  text=forms['out-first-1-countdown-0'] if stores else source
  a=text.index('int GenericToneDetector::process(float *samples, unsigned int n)');z=text.index('\n}',a)+2;fn=text[a:z]
  fn=fn.replace('\tunsigned int k;\n\n\tfor (k = 0; k < n; k++) {','\tunsigned int k;\n\n\tif (n == 0)\n\t\treturn (int)detected;\n\tk = n;\n\tdo {')
  marker='\t}\n\n\treturn (int)detected;'
  assert fn.count(marker)==1
  fn=fn.replace(marker,'\t} while (--k);\n\n\treturn (int)detected;')
  cells[f'guarded-do-out-first-{stores}']=text[:a]+fn+text[z:]
 assert len(set(cells.values()))==3
 return cells
if __name__=='__main__':
 d.REV='902f47fa';d.SOURCE_PATHS=('src/dsp/GenericToneDetector.cpp',);d.OUT_NAME='gcc3-batch50-generic-tone-guard';d.variants=variants;d.main()
