#!/usr/bin/env python3
"""Ten original unsigned-budget/signed-countdown operand boundaries."""
import playbook_small_patterns as d
d.REV='902f47fa';d.SOURCE_PATHS=('src/fax/V17t_prc.c','src/fax/V29t_prc.c');d.OUT_NAME='batch50-fax-budget'
NAMES={'V17t_prc':['TxHdxSCR1V17','TxHdxBridgeV17','TxHdxEQCondV17','TxHdxABV17','TxHdxTEP_V17','TxHdxSilenceV17'], 'V29t_prc':['TxHdxSCR1V29','TxHdxEQCondV29','TxHdxABV29','TxHdxQuietV29']}
def changed(fn,budget,carrier):
 old='n = (remaining <= (short)*budget)';assert old in fn
 if budget:fn=fn.replace(old,'n = (remaining <= (unsigned short)*budget)')
 if carrier:
  assert '\tshort remaining;' in fn
  fn=fn.replace('\tshort remaining;','\tunsigned short remaining;').replace('if (remaining <= 0)','if ((short)remaining <= 0)').replace('n = (remaining <=','n = ((short)remaining <=')
 return fn

def variants(path,source):
 cells={'baseline':source};family=path.split('/')[-1][:-2]
 for budget,carrier,label in [(True,False,'unsigned-budget'),(False,True,'unsigned-carrier'),(True,True,'unsigned-budget-carrier')]:
  text=source
  for name in NAMES[family]:
   a,z,fn=d.function(text,name);text=text[:a]+changed(fn,budget,carrier)+text[z:]
  cells[label]=text
 for label,text in list(cells.items()):
  for name in NAMES[family]:
   a,z,fn=d.function(text,name);old='prm->countdown = (short)(remaining - n);';assert old in fn
   text=text[:a]+fn.replace(old,'prm->countdown -= n;')+text[z:]
  cells[label+'-member-decrement']=text
 return cells
d.variants=variants
if __name__=='__main__':d.main()
