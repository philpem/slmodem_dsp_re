#!/usr/bin/env python3
"""Close a bounded call-result/accumulation-boundary control for V8 biquad."""
import sys
from pathlib import Path
import playbook_small_patterns as driver

def variants(path,source):
 start,end,fn=driver.function(source,'biquad_filter');cells={'baseline':source}
 for label in ('commuted','named-short','named-int'):
  body=fn
  if label!='commuted':body=body.replace('\tint i;', '\tint i;\n\t'+('short' if label=='named-short' else 'int')+' product;')
  for text in ['d->acc_a[i], coeff[i]','d->acc_a[2 + i], coeff[2 + i]']:
   old='\t\tacc += (short)v8_mpyint('+text+');'
   assert body.count(old)==1
   if label=='commuted':new='\t\tacc = (short)v8_mpyint('+text+') + acc;'
   else:new='\t\tproduct = (short)v8_mpyint('+text+');\n\t\tacc += product;'
   body=body.replace(old,new)
  cells[label]=source[:start]+body+source[end:]
 assert len(cells)==len(set(cells.values()))==4
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 driver.REV='80c5dea3';driver.OUT_NAME='gcc3-batch5-v8-biquad';driver.SOURCE_PATHS=('src/v8/V8Detector.c',);driver.variants=variants;driver.main()
