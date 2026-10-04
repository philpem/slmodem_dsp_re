#!/usr/bin/env python3
import playbook_small_patterns as d
d.REV='856c1ecb';d.SOURCE_PATHS=('src/pump/v90/VpcmFloModem.cpp',);d.OUT_NAME='gcc3-batch100-vpcm-cpnot-length-owner'
def variants(path,source):
 start,end,fn=d.function(source,'VPcmFloModem::getV90CpBits');assert fn.count('short live = nofBits;')==1;fn=fn.replace('short live = nofBits;','short live;')
 mark='\t\tfor (i = 0; i < live; i++)';assert fn.count(mark)==1;fn=fn.replace(mark,'\t\tlive = nofBits;\n'+mark)
 return {'baseline':source,'length-after-diagnostic':source[:start]+fn+source[end:]}
d.variants=variants
if __name__=='__main__':d.main()
