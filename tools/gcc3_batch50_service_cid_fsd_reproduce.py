#!/usr/bin/env python3
"""CID rate-specific correlation factoring and native MAC owners."""
import sys,itertools
from pathlib import Path
import playbook_small_patterns as d
def variants(path,source):
 a,z,fn=d.function(source,'CID_FSD_demodulate');cells={'baseline':source}
 select='coef = rate == CID_RATE_9600 ? AUTOCOR_COEF_9600\n\t\t\t\t\t     : AUTOCOR_COEF_7200;'
 mac='acc = 0;\n\t\tk = 0;\n\t\tfor (j = idx; j >= 0; j--)\n\t\t\tacc += cid->ac_hist[j] * coef[k++];\n\t\tfor (j = 4; j > idx; j--)\n\t\t\tacc += cid->ac_hist[j] * coef[k++];'
 assert select in fn and mac in fn
 for arms,cursor in itertools.product([False,True],repeat=2):
  if not(arms or cursor):continue
  f=fn
  if arms:
   f=f.replace(select,'')
   replacement='if (rate == CID_RATE_9600) {\n\t\t\tcoef = AUTOCOR_COEF_9600;\n\t\t\t'+mac+'\n\t\t} else {\n\t\t\tcoef = AUTOCOR_COEF_7200;\n\t\t\t'+mac+'\n\t\t}'
   f=f.replace(mac,replacement)
  if cursor:
   f=f.replace('const short *coef;','const short *coef;\n\t\tconst short *h;')
   f=f.replace('for (j = idx; j >= 0; j--)','h = cid->ac_hist + idx;\n\t\tfor (j = idx; j >= 0; j--)').replace('for (j = 4; j > idx; j--)','h = cid->ac_hist + 4;\n\t\tfor (j = 4; j > idx; j--)').replace('cid->ac_hist[j] * coef[k++]','*h-- * *coef++')
   f=f.replace('k = 0;\n\t\tfor (j = lidx;','k = 0;\n\t\tcoef = fix_LPF;\n\t\th = cid->lpf_hist + lidx;\n\t\tfor (j = lidx;').replace('for (j = 16; j > lidx; j--)','h = cid->lpf_hist + 16;\n\t\tfor (j = 16; j > lidx; j--)').replace('cid->lpf_hist[j] * fix_LPF[k++]','*h-- * *coef++')
  cells['arms-%d-cursor-%d'%(arms,cursor)]=source[:a]+f+source[z:]
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='902f47fa';d.OUT_NAME='gcc3-batch50-service-cid-fsd';d.SOURCE_PATHS=('src/service/Cidfsd.c',);d.variants=variants;d.main()
