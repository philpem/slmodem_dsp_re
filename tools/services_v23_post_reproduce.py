#!/usr/bin/env python3
"""V23 counter post-decrement and structured common tail controls."""
import itertools
import playbook_small_patterns as d
import services_v23_reproduce as prior

def variants(path,source):
 seed=prior.variants(path,source)['zero-1-mute-1-unset-1']
 cells={'baseline':source,'boundary-control':seed}
 for post,structured in itertools.product((False,True),repeat=2):
  if not(post or structured):continue
  t=seed
  if post:
   old='\t\tunsigned short r = tx->remaining;\n\n\t\ttx->remaining = (unsigned short)(r - 1);\n\t\tif (r == 0) {'
   assert t.count(old)==1;t=t.replace(old,'\t\tif (tx->remaining-- == 0) {')
  if structured:
   t=t.replace('\t\tgoto report_count;\n\t}', '\t} else {',1)
   t=t.replace('\nreport_count:\n\t*consumed = done;', '\n\t}\n\t*consumed = done;')
  cells[f'post-{int(post)}-structured-{int(structured)}']=t
 return cells
if __name__=='__main__':
 d.REV='240481e6';d.SOURCE_PATHS=('src/pump/v23/v23tx.c',);d.OUT_NAME='services-v23-post'
 d.DUMP_FLAGS=('-v','-save-temps','-da','-dP');d.variants=variants;d.main()
