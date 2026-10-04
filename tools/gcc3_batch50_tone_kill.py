#!/usr/bin/env python3
"""Cross witnessed tone coefficient scope and output cursor boundary."""
import playbook_small_patterns as d
def variants(path,source):
 a,z,fn=d.function(source,'TONE_kill');cells={}
 for inside in (0,1):
  for post in (0,1):
   q=fn
   if inside:
    q=q.replace('\tconst float *c = t->iir_coef;\n','')
    q=q.replace('\twhile (i--) {','\twhile (i--) {\n\t\tconst float *c = t->iir_coef;')
   if post:q=q.replace('*buf = c[0]','*buf++ = c[0]').replace('\t\tbuf++;\n','')
   label='baseline' if not(inside or post) else f'inside-{inside}-post-{post}'
   cells[label]=source[:a]+q+source[z:]
 assert len(set(cells.values()))==4
 return cells
if __name__=='__main__':
 d.REV='902f47fa';d.SOURCE_PATHS=('src/service/TONE.c',);d.OUT_NAME='gcc3-batch50-tone-kill';d.variants=variants;d.main()
