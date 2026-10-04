#!/usr/bin/env python3
import gcc3_batch50_v90_cp_owner as parent
d=parent.d
d.OUT_NAME='gcc3-batch50-v90-cp-owner-cross'
def variants(path,source):
 base=parent.variants(path,source)
 cells={'baseline':source,'cached-weight':base['cached-weight']}
 for label in ('cached-weight','cached-weight-argument-owner'):
  text=base[label];dest='bits' if label.endswith('argument-owner') else 'p'
  old='if (weight > x) {\n\t\t\t\t*'+dest+' = 0;\n\t\t\t} else {\n\t\t\t\t*'+dest+' = 1;\n\t\t\t\tx -= weight;\n\t\t\t}'
  new='if (!(weight > x)) {\n\t\t\t\t*'+dest+' = 1;\n\t\t\t\tx -= weight;\n\t\t\t} else {\n\t\t\t\t*'+dest+' = 0;\n\t\t\t}'
  assert text.count(old)==2;cells[label+'-subtraction-first']=text.replace(old,new)
 return cells
d.variants=variants
if __name__=='__main__':d.main()
