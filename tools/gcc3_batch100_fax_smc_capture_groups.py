#!/usr/bin/env python3
import playbook_small_patterns as d
d.REV='856c1ecb';d.SOURCE_PATHS=('src/fax/Smc.c',);d.OUT_NAME='gcc3-batch100-fax-smc-capture-groups'
def variants(path,source):
 start,end,fn=d.function(source,'SMC_encoder')
 lo=fn.index('\tconst unsigned short *pmap');hi=fn.index('\tshort *rail_i')
 lines=fn[lo:hi].splitlines(True);assert len(lines)==11
 order=('rot_step','rot_mod','pmask','qmask','amask','qshift','pmap','imap','cosine','sine','qmap')
 by={line.split('=')[0].strip().split()[-1].lstrip('*'):line for line in lines};assert set(by)==set(order)
 fn=fn[:lo]+''.join(by[k] for k in order)+fn[hi:]
 return {'baseline':source,'scalar-before-table-group':source[:start]+fn+source[end:]}
d.variants=variants
if __name__=='__main__':d.main()
