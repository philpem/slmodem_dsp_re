#!/usr/bin/env python3
"""Replay the original INFO1a seven-stage message publication boundary."""
import playbook_small_patterns as d
d.REV='d127aec8';d.OUT_NAME='info1a-publications';d.SOURCE_PATHS=('src/pump/v34/v34info1a.cpp',)
d.DUMP_FLAGS=('-da','-fdump-tree-original')
def variants(path,source):
 old='''\t\tr = 0;
\t\tif (obj->short_abce & 1)
\t\t\tr = 0x80;
\t\tif (obj->short_abce & 2)
\t\t\tr |= 0x40;
\t\tif (obj->short_abd0 & 1)
\t\t\tr |= 0x20;
\t\tif (obj->short_abd0 & 2)
\t\t\tr |= 0x10;
\t\tif (obj->short_abd2 & 1)
\t\t\tr |= 8;
\t\tif (obj->short_abd2 & 2)
\t\t\tr |= 4;
\t\tbits[0] = (short)r;'''
 assert source.count(old)==1
 new=old.replace('\t\tr = 0;','\t\tr = 0;\n\t\tbits[0] = 0;')
 for line in ['r = 0x80;','r |= 0x40;','r |= 0x20;','r |= 0x10;','r |= 8;']:
  new=new.replace('\t\t\t'+line,'\t\t\t'+line+'\n\t\tbits[0] = (short)r;')
 assert new.count('bits[0] =')==7
 return {'baseline':source,'seven-publications':source.replace(old,new)}
d.variants=variants
if __name__ == '__main__':
    d.main()
