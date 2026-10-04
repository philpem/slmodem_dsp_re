#!/usr/bin/env python3
"""Original receive-epoch positive arm, count use and byte flag stores."""
from itertools import product
import playbook_small_patterns as d
d.REV='856c1ecb';d.SOURCE_PATHS=tuple('src/fax/V'+n+'r_prc.c'for n in ('17','27','29'));d.OUT_NAME='batch100-fax-rx-epoch'
def positive(f,n):
 import re
 pat=r'\tif \((?:CarrierDetectV'+n+r'\(modem\) == 0|!CarrierDetectV'+n+r'\(modem\))\) \{'
 m=re.search(pat,f);assert m
 start=m.start();body_start=m.end();end=f.index('\n\t}\n',body_start)
 error=f[body_start:end];assert error.endswith('\n\t\treturn 0;');error=error[:-len('\n\t\treturn 0;')]
 good=f[end+len('\n\t}\n'):];assert good.endswith('\n\treturn 0;\n}')
 good=good[:-len('\n\treturn 0;\n}')]
 return f[:start]+'\tif (CarrierDetectV'+n+'(modem) != 0) {\n'+good+'\n\t} else {'+error+'\n\t}\n\treturn 0;\n}'
def variants(path,s):
 n=path.split('/')[-1][1:3];a,z,f=d.function(s,'RxHdxEpochDetV'+n);cells={}
 dims=3 if n=='17' else 2
 for bits in product((False,True),repeat=dims):
  arm,use=bits[:2];x=f
  if n=='17':
   if use:
    x=x.replace('\tshort left;\n','').replace('left = (short)((unsigned short)RXCTL(modem)->countdown - 1);\n\tRXCTL(modem)->countdown = left;', 'RXCTL(modem)->countdown = (short)((unsigned short)RXCTL(modem)->countdown - 1);').replace('if (left <= 0 ||','if (RXCTL(modem)->countdown <= 0 ||')
   if bits[2]:x=x.replace('EpochDetectV17(modem) != 0','(short)EpochDetectV17(modem) != 0')
  elif n=='27':
   if use:x=x.replace('\tunsigned short left;\n','').replace('left = (unsigned short)(sh->countdown - 1);\n\tsh->countdown = left;', 'sh->countdown = (unsigned short)(sh->countdown - 1);').replace('if ((short)left > 0', 'if ((short)sh->countdown > 0')
  else:
   if use:
    x=x.replace('result.word |= V29_STATUS_ERROR;', 'result.byte.flags |= (unsigned char)(V29_STATUS_ERROR >> 8);').replace('result.word &= ~V29_STATUS_CARRIER;', 'result.byte.flags &= (unsigned char)~(unsigned char)(V29_STATUS_CARRIER >> 8);').replace('result.word |= V29_STATUS_CARRIER;', 'result.byte.flags |= (unsigned char)(V29_STATUS_CARRIER >> 8);')
  if arm:x=positive(x,n)
  names=['carrier-positive','count-member' if n!='29' else 'byte-flags']+(['short-epoch-result']if n=='17' else [])
  l='-'.join(nm for nm,v in zip(names,bits)if v)or'baseline';cells[l]=s[:a]+x+s[z:]
 return cells
d.variants=variants
if __name__=='__main__':d.main()
