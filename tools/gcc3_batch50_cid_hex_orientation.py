#!/usr/bin/env python3
"""Test the unexamined decimal-first arm after the hex read domain closes."""
import itertools,sys
from pathlib import Path
import playbook_small_patterns as d
import gcc3_batch50_cid_hex_reproduce as original

def variants(path,source):
 forms=original.variants(path,source);cells={'baseline':source}
 for late,lowfirst in itertools.product((False,True),repeat=2):
  if not(late or lowfirst):continue
  text=forms['late-1-signed-0-branch-0'] if late else source
  if lowfirst:
   for field in ('hi','lo'):
    old=f'{field} > 9 ? {field} + 0x57 : {field} + 0x30'
    new=f'{field} <= 9 ? {field} + 0x30 : {field} + 0x57'
    assert text.count(old)==1;text=text.replace(old,new)
  cells[f'late-{int(late)}-decimalfirst-{int(lowfirst)}']=text
 assert len(cells)==len(set(cells.values()))==4;return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='902f47fa';d.SOURCE_PATHS=('src/service/Data.c',);d.OUT_NAME='gcc3-batch50-cid-hex-orientation';d.variants=variants;d.main()
