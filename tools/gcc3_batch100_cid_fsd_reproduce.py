#!/usr/bin/env python3
import sys
from pathlib import Path
import playbook_small_patterns as d

def variants(path,source):
 a,z,original=d.function(source,'CID_FSD_demodulate');cells={}
 for ac in (0,1):
  for lp in (0,1):
   for guard in (0,1):
    f=original
    if ac or lp:f=f.replace('\t\tconst short *coef;','\t\tconst short *coef;\n\t\tconst short *history;')
    for enabled,idx,member,coef,top in ((ac,'idx','ac_hist','coef',4),(lp,'lidx','lpf_hist','fix_LPF',16)):
     if not enabled:continue
     old='\t\tacc = 0;\n\t\tk = 0;\n\t\tfor (j = '+idx+'; j >= 0; j--)\n\t\t\tacc += cid->'+member+'[j] * '+coef+'[k++];\n\t\tfor (j = '+str(top)+'; j > '+idx+'; j--)\n\t\t\tacc += cid->'+member+'[j] * '+coef+'[k++];'
     new='\t\tacc = 0;\n\t\thistory = cid->'+member+' + '+idx+';\n'
     if lp:new+='\t\tcoef = fix_LPF;\n'
     new+='\t\tfor (j = '+idx+'; j >= 0; j--)\n\t\t\tacc += *history-- * *coef++;\n\t\thistory += '+str(top+1)+';\n\t\tfor (j = '+str(top)+'; j > '+idx+'; j--)\n\t\t\tacc += *history-- * *coef++;'
     assert f.count(old)==1;f=f.replace(old,new)
    if guard:
     f=f.replace('\tint thresh = cid->thresh;','\tint thresh;\n\n\tif (count == 0)\n\t\treturn 0;\n\tthresh = cid->thresh;')
    label='baseline' if not(ac or lp or guard) else 'ac-%d-lp-%d-guard-%d'%(ac,lp,guard)
    cells[label]=source[:a]+f+source[z:]
 assert len(set(cells.values()))==8
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='856c1ecb';d.OUT_NAME='gcc3-batch100-cid-fsd';d.SOURCE_PATHS=('src/service/Cidfsd.c',);d.variants=variants;d.main()
