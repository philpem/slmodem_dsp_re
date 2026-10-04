#!/usr/bin/env python3
import playbook_small_patterns as d
d.REV='856c1ecb';d.SOURCE_PATHS=('src/fax/SDM.c',);d.OUT_NAME='gcc3-batch100-fax-sdm-output-width'
def variants(path,source):
 start,end,fn=d.function(source,'SDM_scrambler')
 old='unsigned short out = (unsigned short)\n\t\t\t(((reg >> shift1) ^ *data ^ (reg >> shift2)) & mask);';assert fn.count(old)==1;fn=fn.replace(old,'unsigned int out =\n\t\t\t((reg >> shift1) ^ *data ^ (reg >> shift2)) & mask;')
 old='reg = ((reg << nbits) & notmask) | out;';assert fn.count(old)==1;fn=fn.replace(old,'reg = ((reg << nbits) & notmask) | (unsigned short)out;')
 return {'baseline':source,'wide-output-word-uses':source[:start]+fn+source[end:]}
d.variants=variants
if __name__=='__main__':d.main()
