#!/usr/bin/env python3
"""Cross FIFO8's count-result width and element-index update boundaries."""
import itertools,sys
from pathlib import Path
import playbook_small_patterns as d
HEADER=d.ROOT/'include/dsplib/fifo8.h'
def variants(path,source):
 cells={'baseline':source}
 for unsigned,post in itertools.product((False,True),repeat=2):
  if not(unsigned or post):continue
  text=source
  for name in ('FIFO8_read','FIFO8_write'):
   start,end,body=d.function(text,name)
   if unsigned:body=body.replace('return (short)cnt;','return cnt;')
   if post:
    field='rd' if name.endswith('read') else 'wr'
    old='*dst++ = buf[rd];\n\t\trd++;' if field=='rd' else 'buf[wr] = *src++;\n\t\twr++;'
    new='*dst++ = buf[rd++];' if field=='rd' else 'buf[wr++] = *src++;'
    assert body.count(old)==1;body=body.replace(old,new)
   text=text[:start]+body+text[end:]
   if unsigned:
    needle='short\n'+name+'(';assert text.count(needle)==1;text=text.replace(needle,'unsigned short\n'+name+'(')
  cells[f'unsigned-{int(unsigned)}-indexpost-{int(post)}']=text
 assert len(cells)==4 and len(set(cells.values()))==4
 return cells
def headers(path,label):
 if not label.startswith('unsigned-1-'):return {}
 t=HEADER.read_text()
 for name in ('FIFO8_read','FIFO8_write'):
  needle='short '+name+'(';assert t.count(needle)==1;t=t.replace(needle,'unsigned short '+name+'(')
 return {'dsplib/fifo8.h':t}
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='902f47fa';d.SOURCE_PATHS=('src/service/Fifo8.c',);d.OUT_NAME='gcc3-batch50-fifo';d.variants=variants;d.HEADER_OVERLAYS=headers;d.main()
