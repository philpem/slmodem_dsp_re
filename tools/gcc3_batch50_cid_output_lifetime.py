#!/usr/bin/env python3
"""Cross post-memset return buffer lifetime with original owner copy."""
import playbook_small_patterns as d
def variants(path,source):
 a,z,fn=d.function(source,'cid_get_strings');cells={}
 for late in (0,1):
  for direct in (0,1):
   q=fn
   if late:
    q=q.replace('char *out = ctx->strings;','char *out;')
    q=q.replace('sysdep_memset(out, 0, sizeof(ctx->strings));','sysdep_memset(ctx->strings, 0, sizeof(ctx->strings));\n\tout = ctx->strings;')
   if direct:q=q.replace('out[i] =','ctx->strings[i] =')
   label='baseline' if not(late or direct) else f'late-{late}-direct-{direct}'
   cells[label]=source[:a]+q+source[z:]
 assert len(set(cells.values()))==4
 return cells
if __name__=='__main__':
 d.REV='902f47fa';d.SOURCE_PATHS=('src/service/cidcore/cid.c',);d.OUT_NAME='gcc3-batch50-cid-output-lifetime';d.variants=variants;d.main()
