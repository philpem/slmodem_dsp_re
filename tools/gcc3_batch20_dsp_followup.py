#!/usr/bin/env python3
"""Independent narrowing and use boundaries after the first DSP cross."""
import sys,itertools
from pathlib import Path
import playbook_small_patterns as driver
import gcc3_batch20_dsp_reproduce as first

def variants(path,source):
 cells={'baseline':source}
 if path.endswith('fpm_adeq.c'):
  seed=first.variants(path,source)['lms-index-1-dest-1'];start,end,fn=driver.function(seed,'FPM_lmsupd2')
  for late,narrow in itertools.product((False,True),repeat=2):
   body=fn
   if late:
    body=body.replace('\n\t\tshort *dest = c++;','')
    body=body.replace('\n\t\tt = (short)((hist[k--] * mu + 0x10) >> 5);','\n\t\tshort *dest;\n\t\tt = (short)((hist[k--] * mu + 0x10) >> 5);\n\t\tdest = c++;')
   if narrow:body=body.replace('\tint t;', '\tshort t;')
   cells[f'lms2-late-{int(late)}-short-{int(narrow)}']=seed[:start]+body+seed[end:]
  # Isolate the proved first function, without perturbing the second function.
  a,z,fn=driver.function(seed,'FPM_lmsupd');a0,z0,_=driver.function(source,'FPM_lmsupd')
  cells['lms1-only']=source[:a0]+fn+source[z0:]
 else:
  seed=first.variants(path,source)['iir-count-1-cursor-1-out-0'];start,end,fn=driver.function(seed,'FPM_iir_filt')
  for post,ffshort,inputshort in itertools.product((False,True),repeat=3):
   body=fn
   if post:body=body.replace('for (i = (short)(sections - 1); i != -1; i--)','for (i = sections; i--; )')
   if ffshort:body=body.replace('\t\tint ff;', '\t\tshort ff;')
   if inputshort:body=body.replace('\tint acc_in = x;', '\tshort acc_in = x;')
   cells[f'iir-post-{int(post)}-ff-{int(ffshort)}-input-{int(inputshort)}']=seed[:start]+body+seed[end:]
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 driver.REV='93d7eee1';driver.OUT_NAME='gcc3-batch20-dsp-followup';driver.SOURCE_PATHS=('src/dsp/fpm_iir.c','src/dsp/fpm_adeq.c');driver.variants=variants;driver.main()
