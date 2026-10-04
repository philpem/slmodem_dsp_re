#!/usr/bin/env python3
"""HDLC original valid-frame guard and word result-use boundary."""
from itertools import product
import playbook_small_patterns as d
d.REV='902f47fa';d.SOURCE_PATHS=('src/fax/cHDLCrx.c',);d.OUT_NAME='batch50-fax-hdlc-frame-guard'
def variants(path,source):
 a,z,fn=d.function(source,'_hdlc_receive_state');cells={}
 for arm,word in product((False,True),repeat=2):
  f=fn
  if word:
   f=f.replace('unsigned short len = *(unsigned short *)(void *)ctx;','short len = (short)*(unsigned short *)(void *)ctx;').replace('(unsigned char *)(long)word3, len, 1);','(unsigned char *)(long)word3, (unsigned short)len, 1);')
  if arm:
   start=f.index('\t\t\tif (len == 0) {');mid=f.index('\t\t\t} else {',start);end=f.index('\n\t\t\tctx->state = CLASS1_HDLC_RECEIVE_BETWEEN_BUFFERS_STATE;',mid)
   tail=f[:end];assert tail.endswith('\n\t\t\t}')
   fail=f[start+len('\t\t\tif (len == 0) {'):mid];valid=f[mid+len('\t\t\t} else {'):end-len('\n\t\t\t}')]
   f=f[:start]+'\t\t\tif (len != 0) {'+valid+'\n\t\t\t} else {'+fail+'\n\t\t\t}'+f[end:]
  label='-'.join(n for n,v in [('valid-first',arm),('word-carrier',word)] if v) or 'baseline';cells[label]=source[:a]+f+source[z:]
 return cells
d.variants=variants
if __name__=='__main__':d.main()
