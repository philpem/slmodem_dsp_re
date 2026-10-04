#!/usr/bin/env python3
"""Native verifier probe accessor vs manual reciprocal under actual CXX flags."""
import sys
from pathlib import Path
import playbook_small_patterns as driver

def variants(path,source):
 text=source
 for family in ('ISDN','GERMAN_PBX'):
  start=text.index('\t{\n\t\tfloat binsPerHz = 1.0f / binWidth;');end=text.index('\n\t}',start)+3
  old=text[start:end];assert family+'_NULL_FREQ' in old
  args=lambda key:'getSpectrumOfNearestBin(\n\t    params->SPECTRAL_VERIFIER_'+family+'_'+key+'_FREQ)'
  block='\tleftDelta = '+args('LEFT_PEAK')+' -\n\t    '+args('NULL')+';\n\trightDelta = '+args('RIGHT_PEAK')+' -\n\t    '+args('NULL')+';'
  text=text[:start]+block+text[end:]
 start=text.index('\t{\n\t\tfloat binsPerHz = 1.0f / binWidth;');end=text.index('\n\t}',start)+3
 assert 'SEVERE_CODEC_REF_FREQ' in text[start:end]
 block='\n'.join('\t'+v+' = getSpectrumOfNearestBin(\n\t    params->SPECTRAL_VERIFIER_SEVERE_CODEC_'+f+');' for v,f in [('ref','REF_FREQ'),('test1','TEST_FREQ1'),('test2','TEST_FREQ2')])
 text=text[:start]+block+text[end:]
 return {'baseline':source,'native-accessors':text}
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 driver.REV='93d7eee1';driver.OUT_NAME='gcc3-batch20-verifier-native';driver.SOURCE_PATHS=('src/pump/v90/V90SpectralVerifier.cpp',);driver.variants=variants;driver.main()
