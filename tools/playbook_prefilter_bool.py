#!/usr/bin/env python3
"""Cross evaluation and bool-result conversion without changing the int API."""
import playbook_small_patterns as driver

def variants(path,source):
 start,end,fn=driver.function(source,'V90PreFilter::isV90WithEia6')
 old='return (cap == 1) || (V90PW(params)[0x500 / 4] == 6);'
 assert fn.count(old)==1
 cells={'baseline':source}
 for label,operator,intermediate in (('eager-or','|',False),('bool-logical','||',True),('bool-eager','|',True)):
  expression='(cap == 1) '+operator+' (V90PW(params)[0x500 / 4] == 6)'
  replacement=('bool eia6 = '+expression+';\n\treturn eia6;') if intermediate else 'return '+expression+';'
  cells[label]=source[:start]+fn.replace(old,replacement)+source[end:]
 return cells

if __name__=='__main__':
 driver.REV='0c5d71b4'
 driver.OUT_NAME='playbook-prefilter-bool'
 driver.SOURCE_PATHS=('src/pump/v90/V90PreFilter.cpp',)
 driver.variants=variants
 driver.main()
