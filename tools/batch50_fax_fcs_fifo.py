#!/usr/bin/env python3
"""Bounded nibble conversion and FIFO min owner source controls."""
import playbook_small_patterns as d
d.REV='902f47fa';d.SOURCE_PATHS=('src/fax/faxvmi_utls.c','src/fax/fifo.c');d.OUT_NAME='batch50-fax-fcs-fifo'
def variants(path,source):
 cells={'baseline':source}
 for boundary,post,label in [(True,False,'boundary'),(False,True,'postdec'),(True,True,'boundary-postdec')]:
  f=source
  if 'utls' in path:
   if boundary:
    old='(((((fcs) ^ (t_ >> 11)) << 4) ^ t_) | (t_ >> 12))'
    new='((unsigned short)((((fcs) ^ (t_ >> 11)) << 4) ^ t_) | (t_ >> 12))'
    assert old in f;f=f.replace(old,new)
   if post:
    a,z,fn=d.function(f,'faxvmi_gen_fcs16');fn=fn.replace('for (i = count; i != 0; i--)','for (i = count; i-- != 0;)');f=f[:a]+fn+f[z:]
  else:
   a,z,fn=d.function(f,'FIFO_write')
   if boundary:
    fn=fn.replace('unsigned short put = count;', 'unsigned short put = avail;').replace('if (put > avail)\n\t\tput = avail;', 'if (count <= avail)\n\t\tput = count;')
   if post:fn=fn.replace('for (i = put; i != 0; i--)','for (i = put; i-- != 0;)')
   f=f[:a]+fn+f[z:]
  cells[label]=f
 return cells
d.variants=variants
if __name__=='__main__':d.main()
