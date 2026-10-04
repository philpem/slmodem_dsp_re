#!/usr/bin/env python3
import sys
from pathlib import Path
import playbook_small_patterns as d

def variants(path,source):
 cells={}
 for tables in ('retained','extended-xor','extended-add'):
  for counter in (0,1):
   for metric in (0,1):
    s=source
    a,z,f=d.function(s,'vtb_branch')
    if counter:
     assert f.count('\tint k, p, di, dq, e;')==1;f=f.replace('\tint k, p, di, dq, e;','\tshort k;\n\tint p, di, dq, e;')
    if tables!='retained':
     assert f.count('\t\tpt[k] = (short)p;')==1;f=f.replace('\t\tpt[k] = (short)p;','\t\tpt[k] = pt[k + 4] = (short)p;')
     assert f.count('\t\tbm[k] = (short)((e * 0x666) >> 12);')==1;f=f.replace('\t\tbm[k] = (short)((e * 0x666) >> 12);','\t\tbm[k] = bm[k + 4] = (short)((e * 0x666) >> 12);')
    s=s[:a]+f+s[z:]
    a,z,f=d.function(s,'vtb_acs')
    if counter:
     assert f.count('\tint k, d;')==1;f=f.replace('\tint k, d;','\tshort k;\n\tint d;')
    if metric:
     assert f.count('\tint m =')==1;f=f.replace('\tint m =','\tshort m =')
     marker='\tint d;' if counter else '\tint k, d;';assert f.count(marker)==1
     f=f.replace(marker,'\tshort d;' if counter else '\tint k;\n\tshort d;')
    if tables=='extended-add':
     assert f.count('bm[k ^ x]')==1;f=f.replace('bm[k ^ x]','bm[x == 2 ? k + 2 : k ^ x]')
     assert f.count('pt[(best - p0) ^ x]')==1;f=f.replace('pt[(best - p0) ^ x]','pt[x == 2 ? best - p0 + 2 : (best - p0) ^ x]')
    s=s[:a]+f+s[z:]
    if tables!='retained':
     assert s.count('short pt[4], bm[4];')==1;s=s.replace('short pt[4], bm[4];','short pt[8], bm[8];')
    label='baseline' if tables=='retained' and not(counter or metric) else tables+'-counter-%d-metric-%d'%(counter,metric)
    cells[label]=s
 assert len(set(cells.values()))==12
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='856c1ecb';d.OUT_NAME='gcc3-batch100-vtb-extended';d.SOURCE_PATHS=('src/dsp/fpm_vtb.c',);d.variants=variants;d.main()
