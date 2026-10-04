#!/usr/bin/env python3
"""Restore object-supported composite B103 creation diagnostic."""
import playbook_small_patterns as d
d.REV='93d7eee1';d.SOURCE_PATHS=('src/pump/b103/B103prc.c',);d.OUT_NAME='batch20-b103-constructor'
def variants(path,source):
 a,b,fn=d.function(source,'B103FP_create');marker='\n\treturn fp;'
 assert fn.count(marker)==1
 fn=fn.replace(marker,'\n\tif (DSPLIB_DEBUG_ON())\n\t\tdsplibs_debug_printf("B103FP version: %s %s\\n",\n\t\t\t\t     "Sep 22 2005", "15:48:11");'+marker)
 return {'baseline':source,'debug':source[:a]+fn+source[b:]}
d.variants=variants
if __name__=='__main__':d.main()
