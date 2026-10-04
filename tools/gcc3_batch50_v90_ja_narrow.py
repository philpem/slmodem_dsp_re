#!/usr/bin/env python3
from pathlib import Path
import playbook_small_patterns as d
d.REV='902f47fa';d.SOURCE_PATHS=('src/pump/v90/V92Phase3Modulator.cpp',);d.OUT_NAME='gcc3-batch50-v90-ja-narrow'
def variants(path,source):
 cells={'baseline':source};stable=(d.ROOT/'build/gcc3-batch50-v90-cycle/V92Phase3Modulator/covered-common/V92Phase3Modulator.cpp').read_text()
 for stem,text in [('independent',source),('cycles',stable)]:
  start,end,fn=d.function(text,'jaSymbol')
  signed=fn.replace('short sample;','int sample;').replace('sample = (short)-sample;','sample = -sample;').replace('return sample;','return (short)sample;')
  unsigned=fn.replace('short sample;','unsigned short sample;').replace('return sample;','return (short)sample;')
  cells[stem+'-int-postcast']=text[:start]+signed+text[end:]
  cells[stem+'-ushort-postcast']=text[:start]+unsigned+text[end:]
  direct=text;ls,le,lf=d.function(direct,'V92Phase3Modulator::generateJa');body=signed[signed.index('\n{'):].replace('m->','');direct=direct[:ls]+lf[:lf.index('\n{')]+body+direct[le:]
  cells[stem+'-direct-int-postcast']=direct
 return cells
d.variants=variants
if __name__=='__main__':d.main()
