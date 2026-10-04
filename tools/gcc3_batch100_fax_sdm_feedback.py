#!/usr/bin/env python3
import playbook_small_patterns as d
d.REV='856c1ecb';d.SOURCE_PATHS=('src/fax/SDM.c',);d.OUT_NAME='gcc3-batch100-fax-sdm-feedback'
def variants(path,source):
 cells={'baseline':source}
 for scram,descram,label in [(True,False,'scrambler-feedback-first'),(False,True,'descrambler-feedback-first'),(True,True,'both-feedback-first')]:
  text=source
  if scram:
   old='((reg >> shift1) ^ *data ^ (reg >> shift2))';assert text.count(old)==1;text=text.replace(old,'((reg >> shift1) ^ (reg >> shift2) ^ *data)')
  if descram:
   old='((reg >> shift1) ^ in ^ (reg >> shift2))';assert text.count(old)==1;text=text.replace(old,'((reg >> shift1) ^ (reg >> shift2) ^ in)')
  cells[label]=text
 return cells
d.variants=variants
if __name__=='__main__':d.main()
