#!/usr/bin/env python3
"""Measure explicit shift mask absent from original DFT object, without adoption."""
import sys
from pathlib import Path
import playbook_small_patterns as d

def variants(path,source):
 a,z,fn=d.function(source,'dftenergy');assert fn.count('(unsigned char)scale & 31')==1;fn=fn.replace('(unsigned char)scale & 31','(unsigned char)scale')
 return {'baseline':source,'unmasked-original':source[:a]+fn+source[z:]}
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='902f47fa';d.OUT_NAME='gcc3-batch50-v34-dft-shift';d.SOURCE_PATHS=('src/pump/v34/DFTC.c',);d.variants=variants;d.main()
