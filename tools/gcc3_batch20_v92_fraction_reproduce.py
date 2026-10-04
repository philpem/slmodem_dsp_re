#!/usr/bin/env python3
"""V92 float fraction expression versus explicit extended operands."""
import sys
from pathlib import Path
import playbook_small_patterns as driver

def variants(path,source):
 old='((long double)v - (long double)(int)v)';assert source.count(old)==1
 return {'baseline':source,'float-fraction':source.replace(old,'(v - (float)(int)v)')}
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 driver.REV='93d7eee1';driver.OUT_NAME='gcc3-batch20-v92-fraction';driver.SOURCE_PATHS=('src/pump/v90/V92MappingParamsInt.cpp',);driver.variants=variants;driver.main()
