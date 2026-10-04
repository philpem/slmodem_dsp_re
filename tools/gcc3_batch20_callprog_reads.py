#!/usr/bin/env python3
"""Recover two observed diagnostic parameter-read boundaries in CALLPROG_Dial."""
import sys,itertools
from pathlib import Path
import playbook_small_patterns as driver

def variants(path,source):
 a,z,fn=driver.function(source,'CALLPROG_Dial');cells={'baseline':source}
 old='''\tif (DSPLIB_DEBUG_ON())
\t\tdsplibs_debug_printf("GetNoAnswerTimeOut. %d\\n",
\t\t\t\t     cp->timeout[5]);

'''
 assert fn.count(old)==1
 for before,blind in itertools.product((False,True),repeat=2):
  if not (before or blind):continue
  body=fn
  if before:
   body=body.replace(old,'')
   marker='\tcp->fatal = DialerCreate(&cp->dialer, s, cp->modem);'
   body=body.replace(marker,marker+'\n\n'+old.replace('cp->timeout[5]', 'modem_get_param(cp->modem, GetNoAnswerTimeOut)').rstrip())
  if blind:
   marker='''\t\t\t\t"BlindCall: GetBlindDialPause = %d .\\n",
\t\t\t\ttimeout_table[1]);'''
   assert body.count(marker)==1
   body=body.replace(marker, marker.replace('timeout_table[1]', 'modem_get_param(cp->modem, GetBlindDialPause)'))
  cells[f'noanswer-read-{int(before)}-blind-read-{int(blind)}']=source[:a]+body+source[z:]
 assert len(cells)==len(set(cells.values()))==4
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 driver.REV='93d7eee1';driver.OUT_NAME='gcc3-batch20-callprog-reads';driver.SOURCE_PATHS=('src/callprog/Callprog.c',);driver.variants=variants;driver.main()
