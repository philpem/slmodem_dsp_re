#!/usr/bin/env python3
import playbook_small_patterns as d
d.REV='902f47fa'
d.SOURCE_PATHS=('src/pump/v90/V90MappingParamsInt.cpp',)
d.OUT_NAME='gcc3-batch50-v90-masks'
def variants(path,source):
 old='unsigned int c = (unsigned int)(which < 6 ? which : 0);'
 assert source.count(old)==2
 return {'baseline':source, **{label:source.replace(old,new) for label,new in [
 ('boolean-product','unsigned int c = (unsigned int)(which * (which < 6));'),
 ('boolean-mask','unsigned int c = (unsigned int)which & (unsigned int)-(which < 6);'),
 ('explicit-clamp','unsigned int c = (unsigned int)which;\n\tif (which >= 6) c = 0;')]}}
d.variants=variants
if __name__=='__main__':d.main()
