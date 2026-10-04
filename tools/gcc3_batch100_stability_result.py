#!/usr/bin/env python3
"""Cross original zero-result carrier with source comparison orientation."""
import itertools
import playbook_small_patterns as d
def variants(path,source):
 cells={}
 for common,reverse in itertools.product((False,True),repeat=2):
  text=source;label='baseline' if not(common or reverse) else 'common-%d-reverse-%d'%(common,reverse)
  for name in ('check_for_valid','check_for_valid_easy'):
   a,z,fn=d.function(text,name)
   if common:
    fn=fn.replace('\tunsigned short v = w[0];','\tint valid = 0;\n\tunsigned short v = w[0];').replace('\t\treturn 0;','\t\tgoto done;').replace('\treturn 1;','\tvalid = 1;\ndone:\n\treturn valid;')
   if reverse:
    import re
    fn=re.sub(r'v (==|!=) (w\[\d\])',lambda m:m[2]+' '+m[1]+' v',fn)
   text=text[:a]+fn+text[z:]
  cells[label]=text
 return cells
if __name__=='__main__':
 d.REV='856c1ecb';d.SOURCE_PATHS=('src/service/Beepgen.c',);d.OUT_NAME='gcc3-batch100-stability-result';d.variants=variants;d.main()
