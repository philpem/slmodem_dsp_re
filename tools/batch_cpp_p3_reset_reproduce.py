#!/usr/bin/env python3
import playbook_small_patterns as d

def variants(path,source):
 old='''\tif (sessionFlag == 0) {
\t\tjdBits = jd != NULL ? jd->getBitVector() : NULL;
\t} else if (jd92 != NULL) {
\t\tjdV92Bits = jd92->getJdBitVector();
\t\tjdV92PhaseBits = jd92->getJdPhaseBitVector();
\t} else {
\t\tjdV92Bits = NULL;
\t\tjdV92PhaseBits = NULL;
\t}

\tresetDILGenerator(d);'''
 new='''\tif (sessionFlag == 0) {
\t\tjdBits = jd != NULL ? jd->getBitVector() : NULL;
\t\tresetDILGenerator(d);
\t} else {
\t\tif (jd92 != NULL) {
\t\t\tjdV92Bits = jd92->getJdBitVector();
\t\t\tjdV92PhaseBits = jd92->getJdPhaseBitVector();
\t\t} else {
\t\t\tjdV92Bits = NULL;
\t\t\tjdV92PhaseBits = NULL;
\t\t}
\t\tresetDILGenerator(d);
\t}'''
 assert source.count(old)==1
 return {'baseline':source,'arm-local-dil':source.replace(old,new)}
if __name__=='__main__':
 d.REV='276d30b5';d.SOURCE_PATHS=('src/pump/v90/V90Phase3Modulator.cpp',);d.OUT_NAME='batch-cpp-p3-reset';d.DUMP_FLAGS=('-v','-save-temps','-da','-dP');d.variants=variants;d.main()
