#!/usr/bin/env python3
import playbook_small_patterns as d

def variants(path,source):
 old='''\tfor (i = 0; i < 6; i++) {
\t\thead[i] = (unsigned int *)p->constellations[i];
\t\ttableB[i] = (int)p->LC[i];
\t}

\tfor (i = 0; i < 12; i++)
\t\ttableA[i] = p->m[i];'''
 assert source.count(old)==1
 cells={'baseline':source}
 for label,rev in [('expanded-retained',False),('expanded-original-first',True)]:
  lines=[]
  for i in range(6):
   pair=['\thead[%d] = (unsigned int *)p->constellations[%d];'%(i,i),'\ttableB[%d] = (int)p->LC[%d];'%(i,i)]
   lines.extend(pair[::-1] if rev else pair)
  lines+=['\ttableA[%d] = p->m[%d];'%(i,i) for i in range(12)]
  cells[label]=source.replace(old,'\n'.join(lines)).replace('\tint i;\n\n\tif (DSPLIB_DEBUG_ON())','\n\tif (DSPLIB_DEBUG_ON())',1)
 groups=['\ttableB[%d] = (int)p->LC[%d];'%(i,i) for i in range(6)]
 groups+=['\thead[%d] = (unsigned int *)p->constellations[%d];'%(i,i) for i in range(6)]
 groups+=['\ttableA[%d] = p->m[%d];'%(i,i) for i in range(12)]
 cells['expanded-copy-groups']=source.replace(old,'\n'.join(groups)).replace('\tint i;\n\n\tif (DSPLIB_DEBUG_ON())','\n\tif (DSPLIB_DEBUG_ON())',1)
 return cells
if __name__=='__main__':
 d.REV='240481e6';d.SOURCE_PATHS=('src/pump/v90/V92Precoder.cpp',);d.OUT_NAME='batch-cpp-precoder';d.DUMP_FLAGS=('-v','-save-temps','-da','-dP');d.variants=variants;d.main()
