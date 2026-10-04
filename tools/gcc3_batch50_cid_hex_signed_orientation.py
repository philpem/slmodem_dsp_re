#!/usr/bin/env python3
"""Finish predicate-dependent byte signedness on the late-read hex seed."""
import itertools,sys
from pathlib import Path
import playbook_small_patterns as d
import gcc3_batch50_cid_hex_reproduce as original

def variants(path,source):
 cells={'baseline':source};forms=original.variants(path,source)
 for signed,lowfirst in itertools.product((False,True),repeat=2):
  text=forms[f'late-1-signed-{int(signed)}-branch-0']
  if lowfirst:
   for field in ('hi','lo'):
    old=f'{field} > 9 ? {field} + 0x57 : {field} + 0x30';new=f'{field} <= 9 ? {field} + 0x30 : {field} + 0x57'
    assert text.count(old)==1;text=text.replace(old,new)
  cells[f'late-signed-{int(signed)}-decimalfirst-{int(lowfirst)}']=text
 assert len(cells)==len(set(cells.values()))==5;return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='902f47fa';d.SOURCE_PATHS=('src/service/Data.c',);d.OUT_NAME='gcc3-batch50-cid-hex-signed-orientation';d.variants=variants;d.main()
