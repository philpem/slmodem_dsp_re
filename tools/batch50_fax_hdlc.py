#!/usr/bin/env python3
"""Authentic diagnostic and call-visible member index controls."""
import playbook_small_patterns as d
d.REV='902f47fa';d.SOURCE_PATHS=('src/fax/cHDLCrx.c',);d.OUT_NAME='batch50-fax-hdlc'
def variants(path,source):
 cells={'baseline':source};a,z,fn=d.function(source,'_hdlc_emulate_receive_state')
 for diagnostic,member,label in [(True,False,'diagnostic'),(False,True,'member'),(True,True,'diagnostic-member')]:
  f=fn
  if diagnostic:
   old='\t\t\tint n;\n\n\t\t\tfor (i = 0;'
   new='\t\t\tint n;\n\n\t\t\tif (dsplibs_debug_level > 1)\n\t\t\t\tdsplibs_debug_printf("%2d.%02d[sec] Receive buffer OK in _hdlc_emulate_receive_state\\n", ctx->clock_sec, ctx->clock_frac);\n\n\t\t\tfor (i = 0;'
   assert old in f;f=f.replace(old,new)
  if member:
   f=f.replace('\tint next;\n','').replace('\tnext = ctx->superframe_read_idx;\n','').replace('\t\t\tnext++;\n\t\t\tctx->superframe_read_idx = next;', '\t\t\tctx->superframe_read_idx++;')
   import re
   f=re.sub(r'\bnext\b','ctx->superframe_read_idx',f)
  cells[label]=source[:a]+f+source[z:]
 return cells
d.variants=variants
if __name__=='__main__':d.main()
