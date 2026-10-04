#!/usr/bin/env python3
"""Full-TU control for realfft separation temporary assignment width."""
import sys
from pathlib import Path
import playbook_small_patterns as driver

def variants(path, source):
 old='\tdouble h1r, h1i, h2r, h2i;'
 assert source.count(old)==1
 return {'baseline':source,'float-separation':source.replace(old,'\tfloat h1r, h1i, h2r, h2i;')}

if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 driver.REV='93d7eee1';driver.OUT_NAME='gcc3-batch20-fft-width';driver.SOURCE_PATHS=('src/dsp/fft.cpp',);driver.variants=variants;driver.main()
