#!/usr/bin/env python3
"""Original tone result width crossed with escaped phase updates."""
import sys,subprocess,itertools
from pathlib import Path
import playbook_small_patterns as d
def parts(label):
 if label=='baseline':return 'void',False
 return label.split('-')[1],bool(int(label.split('-')[3]))
def overlays(path,label):
 typ,phase=parts(label)
 if typ=='void':return {}
 h=subprocess.check_output(['git','show','902f47fa:include/dsplib/fdspkrnl.h'],cwd=d.ROOT,text=True)
 assert h.count('void TONE_generate(')==1
 return {'dsplib/fdspkrnl.h':h.replace('void TONE_generate(',typ+' TONE_generate(')}
def variants(path,source):
 a,z,fn=d.function(source,'TONE_generate');f=fn.replace('return;','return n;');f=f[:-1]+'\treturn n;\n}'
 old='tm = (float)n * 0.125f + t->elapsed;'
 assert old in f
 seed=(source[:a]+f+source[z:]).replace('\nvoid\nTONE_generate(','\nshort\nTONE_generate(')
 cells={'baseline':source,'result-short-phase-0':seed}
 for cast in [False,True]:
  expr='n * 0.125 + t->elapsed'
  if cast:expr='(float)('+expr+')'
  q=f.replace(old,'tm = '+expr+';')
  cells['result-short-phase-'+str(3+int(cast))]=(source[:a]+q+source[z:]).replace('\nvoid\nTONE_generate(','\nshort\nTONE_generate(')
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='902f47fa';d.OUT_NAME='gcc3-batch50-service-tone-timer';d.SOURCE_PATHS=('src/service/TONE.c',);d.HEADER_OVERLAYS=overlays;d.variants=variants;d.main()
