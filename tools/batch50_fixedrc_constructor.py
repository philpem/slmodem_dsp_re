#!/usr/bin/env python3
"""Original FixedRC constructor store extent and obsolete bank data removal."""
from itertools import product
import playbook_small_patterns as d
d.REV='902f47fa';d.SOURCE_PATHS=('src/core/FixedRC.c',);d.OUT_NAME='batch50-fixedrc-constructor'
def variants(path,source):
 cells={}
 for remove,split in product((False,True),repeat=2):
  text=source
  if remove:
   start=text.index('const struct rc_bank rc_banks[');end=text.index('\n};',start)+len('\n};');text=text[:start]+text[end:]
  if split:
   a,z,fn=d.function(text,'RcFixed_Create')
   old='\t} else {\n\t\th->kind = 0;\n\t\tif (mode < RCFIXED_NMODES) {';assert fn.count(old)==1
   fn=fn.replace(old,'\t} else {\n\t\tif (mode < RCFIXED_NMODES) {\n\t\t\th->kind = 0;')
   old='\t\t\ts->down = (short)fixedRc_DownFact[mode];\n\t\t}\n\t}';assert fn.count(old)==1
   fn=fn.replace(old,'\t\t\ts->down = (short)fixedRc_DownFact[mode];\n\t\t} else {\n\t\t\th->kind = 0;\n\t\t}\n\t}')
   text=text[:a]+fn+text[z:]
  label='-'.join(n for n,v in [('remove-bank',remove),('split-kind-store',split)] if v) or 'baseline';cells[label]=text
 return cells
d.variants=variants
if __name__=='__main__':
 # Dedicated driver adaptation: this declared data-only control intentionally
 # removes exactly one extra global export. Retain the actual symbol census.
 import inspect
 main_source=inspect.getsource(d.main)
 old="assert entry['globals']==base['globals'] and entry['functions']==base['functions']"
 new="expected_globals={n:v for n,v in base['globals'].items() if n!='rc_banks' or 'remove-bank' not in label}; assert entry['globals']==expected_globals and entry['functions']==base['functions']"
 assert main_source.count(old)==1
 exec(main_source.replace(old,new),d.__dict__)
 d.main()
