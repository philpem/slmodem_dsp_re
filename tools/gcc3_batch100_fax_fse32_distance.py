#!/usr/bin/env python3
import playbook_small_patterns as d
d.REV='856c1ecb';d.SOURCE_PATHS=('src/fax/V17rxdec.c',);d.OUT_NAME='gcc3-batch100-fax-fse32-distance'
def variants(path,source):
 start,end,fn=d.function(source,'FAX_FSE_decision_32pt');old='rq >= DECv17_ANA_QMAP[k]';assert fn.count(old)==1;fn=fn.replace(old,'(rq - DECv17_ANA_QMAP[k]) >= 0')
 return {'baseline':source,'distance-sign-predicate':source[:start]+fn+source[end:]}
d.variants=variants
if __name__=='__main__':d.main()
