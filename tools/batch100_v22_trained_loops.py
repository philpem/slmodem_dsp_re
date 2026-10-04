#!/usr/bin/env python3
"""Original V22 training recognizer canonical loops and postfix index."""
from itertools import product
import playbook_small_patterns as d
d.REV='856c1ecb';d.SOURCE_PATHS=('src/pump/v22/v22prc.c',);d.OUT_NAME='batch100-v22-trained-loops'
def variants(path,s):
 cells={}
 first='''RxTrained1200(const short *symbols, const unsigned short *count)
{
	unsigned short n = *count;
	short i;

	for (i = 0; i < (int)n; i++) {
		if (symbols[i] != V22_TRAINED_1200_SYMBOL)
			break;
	}
	return i == (int)n;
}'''
 second='''RxTrained2400(const short *symbols, const unsigned short *count)
{
	unsigned short n = *count;
	short run = 0;
	short i = (short)(n - 1);

	while ((int)run < (int)n && symbols[i--] == V22_TRAINED_2400_SYMBOL)
		run++;
	return run > V22_TRAINED_2400_RUN;
}'''
 for one,two in product((False,True),repeat=2):
  x=s
  for enabled,name,new in [(one,'RxTrained1200',first),(two,'RxTrained2400',second)]:
   if enabled:a,z,_=d.function(x,name);x=x[:a]+new+x[z:]
  l='-'.join(n for n,v in [('canonical-1200',one),('postfix-2400',two)]if v)or'baseline';cells[l]=x
 return cells
d.variants=variants
if __name__=='__main__':d.main()
