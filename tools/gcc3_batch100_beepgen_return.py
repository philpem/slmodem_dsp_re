#!/usr/bin/env python3
"""Beep generator allocation common-return boundary, two complete TUs."""
import playbook_small_patterns as d
d.REV='856c1ecb';d.SOURCE_PATHS=('src/service/Beepgen.c',);d.OUT_NAME='gcc3-batch100-beepgen-return'
def variants(path,s):
 a,z,f=d.function(s,'beepgen_create')
 old='\tif (bg == NULL) {\n\t\tbg = sysdep_malloc(sizeof(struct beepgen));\n\t\tif (bg == NULL)\n\t\t\treturn NULL;\n\t}\n'
 assert old in f
 f=f.replace(old,'\tif (bg == NULL)\n\t\tbg = sysdep_malloc(sizeof(struct beepgen));\n\tif (bg != NULL) {\n',1)
 marker='\treturn bg;';assert f.count(marker)==1;f=f.replace(marker,'\t}\n'+marker,1)
 return {'baseline':s,'common-pointer-return':s[:a]+f+s[z:]}
d.variants=variants
if __name__=='__main__':d.main()
