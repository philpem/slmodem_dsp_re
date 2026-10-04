#!/usr/bin/env python3
"""Tone verdict formal width crossed with caller narrowing."""
import sys,subprocess,itertools
from pathlib import Path
import playbook_small_patterns as d
def overlays(path,label):
 if label=='baseline' or label.split('-')[1]=='0':return {}
 h=subprocess.check_output(['git','show','902f47fa:include/dsplib/fdspkrnl.h'],cwd=d.ROOT,text=True)
 assert h.count('int TONE_detect(')==1
 return {'dsplib/fdspkrnl.h':h.replace('int TONE_detect(','short TONE_detect(')}
def variants(path,source):
 cells={}
 for shortret,cast in itertools.product([False,True],repeat=2):
  label='baseline' if not(shortret or cast) else 'short-%d-cast-%d'%(shortret,cast)
  s=source
  if shortret and path.endswith('TONE.c'):
   assert '\nint\nTONE_detect(' in s
   s=s.replace('\nint\nTONE_detect(','\nshort\nTONE_detect(')
  if cast and path.endswith('Detector.c'):
   assert s.count('if (TONE_detect(')==1
   s=s.replace('if (TONE_detect(','if ((short)TONE_detect(')
  cells[label]=s
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='902f47fa';d.OUT_NAME='gcc3-batch50-service-tone-return';d.SOURCE_PATHS=('src/service/TONE.c','src/service/Detector.c');d.HEADER_OVERLAYS=overlays;d.variants=variants;d.main()
