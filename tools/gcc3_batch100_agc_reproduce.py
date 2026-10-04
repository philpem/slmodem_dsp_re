#!/usr/bin/env python3
import sys
from pathlib import Path
import playbook_small_patterns as d

def variants(path,source):
 a,z,original=d.function(source,'FPM_AGC_agc');cells={}
 for cursor in (0,1):
  for word in (0,1):
   for gate in (0,1):
    f=original
    if word:
     assert f.count('\tint i;')==1;f=f.replace('\tint i;','\tshort i;')
    if cursor:
     f=f.replace('\t\tint k;','\t\tunsigned short k;\n\t\tshort *dst;')
     old='\t\t\tfor (k = 0; k < (int)len; k++)\n\t\t\t\tsamples[k] = 0;'
     assert f.count(old)==1;f=f.replace(old,'\t\t\tdst = samples;\n\t\t\tfor (k = len; k--; dst++)\n\t\t\t\t*dst = 0;')
     old='\t\tfor (k = 0; k < (int)len; k++) {';assert f.count(old)==1;f=f.replace(old,'\t\tdst = samples;\n\t\tfor (k = len; k--; dst++) {')
     assert f.count('samples[k]')==4;f=f.replace('samples[k]','*dst')
    if gate:
     for old,new in [('level < acquire_level','(short)level < (short)acquire_level'),('level < squelch_level','(short)level < (short)squelch_level'),('mult == 0','(short)mult == 0'),('mult != 0','(short)mult != 0'),('level_est != 0','(short)level_est != 0')]:
      assert f.count(old)==1;f=f.replace(old,new)
    name='baseline' if not(cursor or word or gate) else 'cursor-%d-word-%d-gate-%d'%(cursor,word,gate)
    cells[name]=source[:a]+f+source[z:]
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='856c1ecb';d.OUT_NAME='gcc3-batch100-agc';d.SOURCE_PATHS=('src/dsp/fpm_agc.c',);d.variants=variants;d.main()
