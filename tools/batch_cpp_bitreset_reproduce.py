#!/usr/bin/env python3
import playbook_small_patterns as d

def variants(path,source):
 old='''\tif (mp->shaperSR != 0)
\t\textraSymbols = V90MAPPER_FRAME * mp->shaperId / mp->shaperSR;
\telse
\t\textraSymbols = 0;'''
 assert source.count(old)==1
 local='''\tunsigned int extra = 0;
\tif (mp->shaperSR != 0)
\t\textra = V90MAPPER_FRAME * mp->shaperId / mp->shaperSR;
\textraSymbols = extra;'''
 conditional='''\textraSymbols = mp->shaperSR != 0
\t    ? V90MAPPER_FRAME * mp->shaperId / mp->shaperSR : 0;'''
 return {'baseline':source,'guarded-result':source.replace(old,local),'conditional':source.replace(old,conditional)}
if __name__=='__main__':
 d.REV='240481e6';d.SOURCE_PATHS=('src/pump/v90/V90bitsToSymbol.cpp',);d.OUT_NAME='batch-cpp-bitreset';d.DUMP_FLAGS=('-v','-save-temps','-da','-dP');d.variants=variants;d.main()
