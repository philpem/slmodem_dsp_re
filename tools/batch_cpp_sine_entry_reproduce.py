#!/usr/bin/env python3
import playbook_small_patterns as d

def variants(path,source):return {'baseline':source,'common-loop':source}
def overlays(path,label):
 if label=='baseline':return {}
 text=(d.ROOT/'include/dsplib/SineWave.h').read_text()
 a=text.index('\tif (n == 0) {',text.index('void SineWave<Tout, Tparam>::generate('));z=text.index('\n\t/*\n\t * The wrap:',a)
 replacement='''\tunsigned long i = 0;
\tx = phase;
\twhile (i < n) {
\t\tlong double s = sinl(x);
\t\tout[i] = (Tout)(s * (long double)amplitude);
\t\t++i;
\t\tx = (long double)phase + step;
\t\tif (i < n)
\t\t\tphase = (Tparam)x;
\t}
'''
 text=text[:a]+replacement+text[z:]
 return {'dsplib/SineWave.h':text}
if __name__=='__main__':
 d.REV='e0052eec';d.SOURCE_PATHS=('src/pump/v90/VpcmFloModem.cpp',);d.OUT_NAME='batch-cpp-sine-entry';d.DUMP_FLAGS=('-v','-save-temps','-da','-dP');d.variants=variants;d.HEADER_OVERLAYS=overlays;d.main()
