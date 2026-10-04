#!/usr/bin/env python3
"""FDSP original reverse block countdown/destination owner/copy target."""
import sys,itertools
from pathlib import Path
import playbook_small_patterns as d
def variants(path,source):
 a,z,fn=d.function(source,'FDSP_Kernel_Loop');cells={'baseline':source}
 for down,cursor,copy in itertools.product([False,True],repeat=3):
  if not(down or cursor or copy):continue
  f=fn
  for owner,inputn in [('a','b'),('b','a')]:
   old='for (i = 0; i < FDSP_BLOCK; i++)\n\t\trev_'+owner+'[i] = in_'+inputn+'[FDSP_BLOCK - 1 - i];\n\tsysdep_memcpy(k->chan_'+owner+'->dly, rev_'+owner+', sizeof(rev_'+owner+'));'
   assert old in f
   lines=['{']
   if copy:lines+=['float *target = k->chan_'+owner+'->dly;']
   if cursor:lines+=['float *dst = rev_'+owner+';']
   if down:lines+=['int j;','for (j = FDSP_BLOCK - 1; j >= 0; j--)']
   else:lines+=['for (i = 0; i < FDSP_BLOCK; i++)']
   lhs='*dst++' if cursor else ('rev_'+owner+'[FDSP_BLOCK - 1 - j]' if down else 'rev_'+owner+'[i]')
   rhs='in_'+inputn+('[j]' if down else '[FDSP_BLOCK - 1 - i]')
   lines+=['\t'+lhs+' = '+rhs+';','sysdep_memcpy('+('target' if copy else 'k->chan_'+owner+'->dly')+', rev_'+owner+', sizeof(rev_'+owner+'));','}']
   f=f.replace(old,'\n\t'.join(lines))
  cells['down-%d-cursor-%d-target-%d'%(down,cursor,copy)]=source[:a]+f+source[z:]
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='902f47fa';d.OUT_NAME='gcc3-batch50-fdsp-loop';d.SOURCE_PATHS=('src/service/Fdspkrnl.c',);d.variants=variants;d.main()
