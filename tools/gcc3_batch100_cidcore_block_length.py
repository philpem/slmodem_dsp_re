#!/usr/bin/env python3
import playbook_small_patterns as d
d.REV='856c1ecb';d.SOURCE_PATHS=('src/service/cidcore/cid.c',);d.OUT_NAME='gcc3-batch100-cidcore-block-length'
def variants(path,source):
 start=source.index('cid_progress(struct cid_modem *ctx');end=source.index('\n}',start)+2;fn=source[start:end];assert fn.count('int len = 0;')==1
 return {'baseline':source,'exhaustive-length':source[:start]+fn.replace('int len = 0;','int len;')+source[end:]}
d.variants=variants
if __name__=='__main__':d.main()
