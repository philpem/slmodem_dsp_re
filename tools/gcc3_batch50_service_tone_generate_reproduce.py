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
 a,z,fn=d.function(source,'TONE_generate');cells={'baseline':source}
 for typ,phase in itertools.product(['short','int'],[False,True]):
  f=fn.replace('return;','return n;')
  f=f[:-1]+'\treturn n;\n}'
  if phase:
   old='p = (float)(ph.phase + 3.141592653589793);\n\tif (p > 6.28318530718)\n\t\tp = (float)(p - 6.28318530718);\n\tph.phase = p;'
   assert old in f
   f=f.replace(old,'ph.phase = (float)(ph.phase + 3.141592653589793);\n\tif (ph.phase > 6.28318530718)\n\t\tph.phase = (float)(ph.phase - 6.28318530718);').replace('float tm, p;','float tm;')
  text=source[:a]+f+source[z:]
  assert '\nvoid\nTONE_generate(' in text
  text=text.replace('\nvoid\nTONE_generate(','\n'+typ+'\nTONE_generate(')
  cells['result-'+typ+'-phase-'+str(int(phase))]=text
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='902f47fa';d.OUT_NAME='gcc3-batch50-service-tone-generate';d.SOURCE_PATHS=('src/service/TONE.c',);d.HEADER_OVERLAYS=overlays;d.variants=variants;d.main()
