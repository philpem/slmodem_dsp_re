#!/usr/bin/env python3
"""Consistent V27 descrambler public count/consumption boundary cross."""
from pathlib import Path
import subprocess
import playbook_small_patterns as d
d.REV='902f47fa'
primary=('src/fax/V27r_int.c','src/fax/V27r_prc.c')
others=[]
for p in sorted((d.ROOT/'src').glob('**/*.c')):
 if 'dsplib/v27fax.h' in p.read_text():
  rel=str(p.relative_to(d.ROOT))
  if rel not in primary and (d.ROOT/'build/tc_out'/(rel.replace('/','_')+'.o')).exists():others.append(rel)
d.SOURCE_PATHS=primary+tuple(others);d.OUT_NAME='batch50-v27-descramble-api'
header=subprocess.check_output(['git','show',d.REV+':include/dsplib/v27fax.h'],cwd=d.ROOT,text=True)
old='void DescrambleDataV27(void *modem, unsigned short *data, short count);';assert header.count(old)==1
candidate=header.replace(old,'void DescrambleDataV27(void *modem, unsigned short *data, unsigned short count);')
d.HEADER_OVERLAYS=lambda path,label: {'dsplib/v27fax.h':candidate} if label.startswith('unsigned') else {}
def variants(path,source):
 cells={'baseline':source}
 labels=('unsigned','no-casts','unsigned-no-casts') if path in primary else ('unsigned',)
 for label in labels:
  text=source
  if path==primary[0] and label.startswith('unsigned'):
   text=text.replace('DescrambleDataV27(void *modem, unsigned short *data, short count)','DescrambleDataV27(void *modem, unsigned short *data, unsigned short count)')
   a,z,fn=d.function(text,'DescrambleDataV27');assert 'data, count);' in fn;fn=fn.replace('data, count);','data, (short)count);');text=text[:a]+fn+text[z:]
  if path==primary[1] and 'no-casts' in label:
   old='DescrambleDataV27(modem, (unsigned short *)(void *)out, (short)n);';assert text.count(old)==2
   text=text.replace(old,'DescrambleDataV27(modem, (unsigned short *)(void *)out, n);')
  cells[label]=text
 return cells
d.variants=variants
if __name__=='__main__':d.main()
