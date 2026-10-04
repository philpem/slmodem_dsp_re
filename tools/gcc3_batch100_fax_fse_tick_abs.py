#!/usr/bin/env python3
import playbook_small_patterns as d
d.REV='856c1ecb';d.SOURCE_PATHS=('src/fax/V17rxdec.c',);d.OUT_NAME='gcc3-batch100-fax-fse-tick-abs'
def variants(path,source):
 cells={'baseline':source}
 for tick,absnode,label in [(True,False,'member-tick'),(False,True,'abs-nodes'),(True,True,'member-tick-abs-nodes')]:
  text=source
  if tick:
   start,end,fn=d.function(text,'fse_tick')
   fn='fse_tick(struct v17_dec *m)\n{\n\tm->sym_count++;\n\tif (m->sym_count == 0x8000)\n\t\tm->sym_count = 0x4000;\n}'
   text=text[:start]+fn+text[end:]
  if absnode:
   start,end,fn=d.function(text,'FAX_FSE_decision_16pt')
   old='n = ((ib < 0 ? -ib : ib) + (qb < 0 ? -qb : qb)) >> 13;';assert fn.count(old)==1
   fn=fn.replace(old,'ib = __builtin_abs(ib);\n\tqb = __builtin_abs(qb);\n\tn = (ib + qb) >> 13;');text=text[:start]+fn+text[end:]
  cells[label]=text
 return cells
d.variants=variants
if __name__=='__main__':d.main()
