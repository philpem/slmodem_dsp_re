#!/usr/bin/env python3
"""V29 receive data/protocol original byte union writes and carrier arm."""
from itertools import product
import re
import playbook_small_patterns as d
from batch100_fax_rx_epoch import variants as epoch_variants
d.REV='856c1ecb';d.SOURCE_PATHS=('src/fax/V29r_prc.c',);d.OUT_NAME='batch100-fax-v29-rx-flags'
def byte_writes(fn):
 def convert(match):
  op, complement, name = match.groups()
  shift = 16 if name == 'IDLE' else 8
  field = 'flags2' if shift == 16 else 'flags'
  mask = '(unsigned char)' + complement + '(V29_STATUS_' + name + ' >> ' + str(shift) + ')'
  return 'result.byte.' + field + ' ' + op + '= ' + mask + ';'
 return re.sub(r'result\.word ([|&])= (~?)V29_STATUS_(\w+);', convert, fn)
def positive(f):
 a=f.index('\tif (!CarrierDetectV29(modem)) {');b=f.index('\n\t}\n',a);error=f[a+len('\tif (!CarrierDetectV29(modem)) {'):b];error=error.removesuffix('\n\t\treturn 0;')
 good=f[b+len('\n\t}\n'):];good=good.removesuffix('\n}')
 return f[:a]+'\tif (CarrierDetectV29(modem) != 0) {\n'+good+'\n\t} else {'+error+'\n\t}\n\treturn 0;\n}'
def variants(path,s):
 cells={};current=epoch_variants(path,s)['carrier-positive-byte-flags']
 for data,protocol,arm in product((False,True),repeat=3):
  label='-'.join(n for n,v in [('data-byte',data),('protocol-byte',protocol),('protocol-positive',arm)]if v)or'baseline'
  x=current if label!='baseline'else s
  for fn,enabled in [('RxHdxDataV29',data),('RxHdxPrtcolV29',protocol)]:
   a,z,f=d.function(x,fn)
   if enabled:f=byte_writes(f)
   if fn=='RxHdxPrtcolV29'and arm:f=positive(f)
   x=x[:a]+f+x[z:]
  cells[label]=x
 return cells
d.variants=variants
if __name__=='__main__':d.main()
