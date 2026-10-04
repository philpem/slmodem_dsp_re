#!/usr/bin/env python3
"""Callprog blind diagnostic order x conditional validation host reread."""
import sys
from pathlib import Path
import playbook_small_patterns as driver
from gcc3_batch20_callprog_reads import variants as prior

def variants(path,source):
 seed=prior(path,source)['noanswer-read-1-blind-read-1'];cells={'baseline':source}
 for before in (0,1):
  for reread in (0,1):
   text=seed
   if before:
    old='\t\ttimeout_table[1] = modem_get_param(cp->modem,\n\t\t\t\t\t\t   GetBlindDialPause);\n\n';assert text.count(old)==1;text=text.replace(old,'')
    marker='\t\t\t\tmodem_get_param(cp->modem, GetBlindDialPause));\n';assert text.count(marker)==1
    text=text.replace(marker,marker+'\n'+old.rstrip()+'\n')
   if reread:
    old='\t\tif (extra <= 2)\n\t\t\textra = 2;\n';assert text.count(old)==1
    text=text.replace(old,old+'\t\telse\n\t\t\textra = (modem_get_param(cp->modem,\n\t\t\t\t\t\t GetDialToneValidationTime) + 9) / 10;\n')
   cells[f'diagnostic-first-{before}-validation-reread-{reread}']=text
 assert len(set(cells.values()))==5
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 driver.REV='93d7eee1';driver.OUT_NAME='gcc3-batch20-callprog-callback';driver.SOURCE_PATHS=('src/callprog/Callprog.c',);driver.variants=variants;driver.main()
