#!/usr/bin/env python3
"""Cross blob shared failure cleanup and reference emission order."""
import playbook_small_patterns as d
from gcc3_batch50_wrapper_creation import variants as prior
def variants(path,source):
 cells={'baseline':source}
 for label,text in prior(path,source).items():
  if label=='baseline':continue
  a,z,fn=d.function(text,'dp_wrapper_create')
  old='\t\t\tdp_wrapper_delete(w);\n\t\t\treturn NULL;'
  assert fn.count(old)==2
  fn=fn.replace(old,'\t\t\tgoto failed;')
  fn=fn.replace('\treturn w;\n}','\treturn w;\n\nfailed:\n\tdp_wrapper_delete(w);\n\treturn NULL;\n}')
  q=text[:a]+fn+text[z:]
  for order in (0,1):
   t=q
   if order:
    a,z,fn=d.function(t,'dp_wrapper_delete');a-=len('void\n')
    assert t[a:a+len('void\n')]=='void\n'
    full=t[a:z];t=t[:a]+t[z:]
    pos=t.index('struct dp_wrapper *\ndp_wrapper_create')
    t=t[:pos]+full+'\n\n'+t[pos:]
   cells[label+('-delete-first' if order else '-shared-failure')]=t
 assert len(set(cells.values()))==5
 return cells
if __name__=='__main__':
 d.REV='902f47fa';d.SOURCE_PATHS=('src/core/dp_wrapper.c',);d.OUT_NAME='gcc3-batch50-wrapper-cleanup';d.variants=variants;d.main()
