#!/usr/bin/env python3
"""Original unsigned-word product carriers x incremental energy updates."""
import sys,itertools
from pathlib import Path
import playbook_small_patterns as d

def variants(path,source):
 a,z,fn=d.function(source,'FPM_TONE_find_rev');cells={}
 for capture,split in itertools.product((0,1),repeat=2):
  body=fn
  if split:body=body.replace('energy += ((samples[n] * samples[n]) >> 15)\n\t\t\t  - ((hist[idx] * hist[idx]) >> 15);','energy += ((samples[n] * samples[n]) >> 15);\n\t\tenergy -= ((hist[idx] * hist[idx]) >> 15);')
  if capture:
   marker='\t\tcorr += ((samples[n] - hist[idx]) * hist[j] + 0x4000) >> 15;'
   assert body.count(marker)==1
   body=body.replace(marker,'\t\t{\n\t\tunsigned short current = samples[n];\n\t\tunsigned short outgoing = hist[idx];\n'+marker)
   begin=body.index('\t\t{\n\t\tunsigned short current');end=body.index('\n\t\tif (2 * corr_s',begin)
   fragment=body[begin:end];decls,products=fragment.split(marker.split('corr +=')[0]+'corr +=',1)
   products=products.replace('samples[n]','(short)current').replace('hist[idx]','(short)outgoing')
   body=body[:begin]+decls+'\t\tcorr +='+products+'\n\t\t}\n'+body[end:]
  label='baseline' if not(capture or split) else f'capture-{capture}-split-{split}'
  cells[label]=source[:a]+body+source[z:]
 assert len(set(cells.values()))==4
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='902f47fa';d.OUT_NAME='gcc3-batch50-tone-reversal';d.SOURCE_PATHS=('src/dsp/fpm_tone.c',);d.variants=variants;d.main()
