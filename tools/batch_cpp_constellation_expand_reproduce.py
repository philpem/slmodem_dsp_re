#!/usr/bin/env python3
import playbook_small_patterns as d

def variants(path,source):
 digit='''\tfor (i = 0; i < V90CP_CONSTELLATIONS - 1; i++) {
\t\tmodulus[i] = (unsigned int)(remaining[i]
\t\t\t     % mappingParams->constellationSize[i]);
\t\tremaining[i + 1] = (remaining[i] - modulus[i])
\t\t\t\t   / mappingParams->constellationSize[i];
\t}'''
 place='''\tfor (i = 1; i < V90CP_CONSTELLATIONS; i++) {
\t\tproduct *= mappingParams->constellationSize[i - 1];
\t\tplaceValue[i] = product;
\t}'''
 assert source.count(digit)==source.count(place)==1
 digits='\n'.join('''\tmodulus[%d] = (unsigned int)(remaining[%d]
\t    %% mappingParams->constellationSize[%d]);
\tremaining[%d] = (remaining[%d] - modulus[%d])
\t    / mappingParams->constellationSize[%d];'''%(i,i,i,i+1,i,i,i) for i in range(5))
 places='\n'.join('''\tproduct *= mappingParams->constellationSize[%d];
\tplaceValue[%d] = product;'''%(i-1,i) for i in range(1,6))
 cells={}
 for dg in (0,1):
  for pl in (0,1):
   text=source.replace(digit,digits) if dg else source
   if pl:text=text.replace(place,places)
   if dg and pl:
    a=text.index('V90ConstellationPower::calcModulusParameters(');z=text.index('\n}\n',a)
    prefix=text[a:z];assert prefix.count('\tunsigned int i;\n')==1;text=text[:a]+prefix.replace('\tunsigned int i;\n','')+text[z:]
   cells['baseline' if not(dg or pl) else 'digits%d-place%d'%(dg,pl)]=text
 text=cells['digits1-place1']
 old='mappingParams->shaperSR\n\t\t\t\t+ mappingParams->word_0 - 6'
 assert text.count(old)==1
 cells['expanded-sum-original-loads']=text.replace(old,'mappingParams->word_0\n\t\t\t\t+ mappingParams->shaperSR - 6')
 return cells
if __name__=='__main__':
 d.REV='6b4509bd';d.SOURCE_PATHS=('src/pump/v90/V90ConstellationPower.cpp',);d.OUT_NAME='batch-cpp-constellation-expand';d.DUMP_FLAGS=('-v','-save-temps','-da','-dP');d.variants=variants;d.main()
