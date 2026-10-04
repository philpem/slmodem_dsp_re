#!/usr/bin/env python3
import playbook_small_patterns as d
d.REV='902f47fa'
d.SOURCE_PATHS=('src/pump/v90/V90PreFilter.cpp',)
d.OUT_NAME='gcc3-batch50-v90-prefilter'
def variants(path,source):
 old="\tV90RefLoop *loops = dataBase[codecType].loops;\n\tint n = 0;"
 assert source.count(old)==1
 early=source.replace(old,"\tint n = 0;\n\tV90RefLoop *loops = dataBase[codecType].loops;")
 oldloop="\twhile (loops[n].name[0] != '\\0')\n\t\tn++;"
 assert early.count(oldloop)==1
 pointer=early.replace(oldloop,"\twhile (loops->name[0] != '\\0') {\n\t\tn++;\n\t\tloops++;\n\t}")
 return {'baseline':source,'count-before-bank':early,'pointer-walk':pointer}
d.variants=variants
if __name__=='__main__':d.main()
