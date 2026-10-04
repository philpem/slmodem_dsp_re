#!/usr/bin/env python3
import playbook_small_patterns as d
d.REV='856c1ecb';d.SOURCE_PATHS=('src/fax/V17rxdec.c',);d.OUT_NAME='gcc3-batch100-fax-fse16-owner'
def variants(path,source):
 cells={'baseline':source}
 for absnode,label in [(False,'input-owners-first'),(True,'input-owners-first-abs-nodes')]:
  start,end,fn=d.function(source,'FAX_FSE_decision_16pt')
  old='\tm = (struct v17_dec *)state->cfg.owner;\n\n\ti = state->out_i[(short)state->n_out];\n\tq = state->out_q[(short)state->n_out];';assert fn.count(old)==1
  fn=fn.replace(old,'\tshort *input_i = state->out_i;\n\tshort *input_q = state->out_q;\n\tshort pos = (short)state->n_out;\n\tm = (struct v17_dec *)state->cfg.owner;\n\n\ti = input_i[pos];\n\tq = input_q[pos];')
  if absnode:
   old='n = ((ib < 0 ? -ib : ib) + (qb < 0 ? -qb : qb)) >> 13;';assert fn.count(old)==1;fn=fn.replace(old,'ib = __builtin_abs(ib);\n\tqb = __builtin_abs(qb);\n\tn = (ib + qb) >> 13;')
  cells[label]=source[:start]+fn+source[end:]
 return cells
d.variants=variants
if __name__=='__main__':d.main()
