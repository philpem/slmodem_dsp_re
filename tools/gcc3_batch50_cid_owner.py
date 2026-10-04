#!/usr/bin/env python3
"""Cross witnessed CID child lifetime and copy destination owner."""
import sys
from pathlib import Path
import playbook_small_patterns as d
def variants(path,source):
 start,end,fn=d.function(source,'cid_get_strings')
 cells={}
 for capture in (0,1):
  for direct in (0,1):
   q=fn
   if capture:
    q=q.replace("\t\tfor (i = 0; i <= 15; i++)","\t\tstruct dtmf_rx *digits_owner = ctx->dtmf;\n\n\t\tfor (i = 0; i <= 15; i++)")
    q=q.replace('ctx->dtmf->digits[i]','digits_owner->digits[i]')
   if direct:q=q.replace('out[i] =','ctx->strings[i] =')
   label='baseline' if not(capture or direct) else f'capture-{capture}-direct-{direct}'
   cells[label]=source[:start]+q+source[end:]
 assert len(set(cells.values()))==4
 return cells
if __name__=='__main__':
 d.REV='902f47fa';d.SOURCE_PATHS=('src/service/cidcore/cid.c',);d.OUT_NAME='gcc3-batch50-cid-owner';d.variants=variants;d.main()
