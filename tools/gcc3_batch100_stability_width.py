#!/usr/bin/env python3
"""Cross C's promoted history word with a common-result comparison graph."""
import itertools
import playbook_small_patterns as d
from gcc3_batch100_stability_result import variants as base_variants
def variants(path,source):
 seeds=base_variants(path,source);cells={'baseline':source,'short-common':seeds['common-1-reverse-0']}
 for common,typ in itertools.product((False,True),('int','unsigned int')):
  text=seeds['common-1-reverse-0'] if common else source
  for name in ('check_for_valid','check_for_valid_easy'):
   a,z,f=d.function(text,name);old='unsigned short v = w[0];';assert old in f
   f=f.replace(old,typ+' v = w[0];');text=text[:a]+f+text[z:]
  cells[('common-' if common else 'literal-')+typ.replace(' ','-')]=text
 return cells
if __name__=='__main__':
 d.REV='856c1ecb';d.SOURCE_PATHS=('src/service/Beepgen.c',);d.OUT_NAME='gcc3-batch100-stability-width';d.variants=variants;d.main()
