#!/usr/bin/env python3
import playbook_small_patterns as d
d.REV='902f47fa'
d.SOURCE_PATHS=('src/pump/v90/V90MappingParamsInt.cpp',)
d.OUT_NAME='gcc3-batch50-v90-mask-index'
def variants(path,source):
 old='\tint k, i;'
 assert source.count(old)==2
 index=source.replace(old,'\tint k;\n\tunsigned int i;')
 clampold='unsigned int c = (unsigned int)(which < 6 ? which : 0);'
 clampnew='unsigned int c = (unsigned int)which;\n\tif (which >= 6) c = 0;'
 return {'baseline':source,'unsigned-index':index,'unsigned-index-clamp':index.replace(clampold,clampnew)}
d.variants=variants
if __name__=='__main__':d.main()
