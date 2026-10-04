#!/usr/bin/env python3
"""Independent original shift-array capture on the declared section/width seed."""
import sys
from pathlib import Path
import playbook_small_patterns as driver
from gcc3_batch20_toneiir_sections_reproduce import variants as seeds

def variants(path,source):
 text=seeds(path,source)['short-taps-1-expanded-sections-1']
 a,z,fn=driver.function(text,'_iir_filter_progress')
 assert fn.count('int i;')==1
 fn=fn.replace('f->shift','shift').replace('int i;','int i;\n\tshort *shift = f->shift;',1)
 return {'baseline':source,'short-sections-shift-capture':text[:a]+fn+text[z:]}
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 driver.REV='93d7eee1';driver.OUT_NAME='gcc3-batch20-toneiir-shift';driver.SOURCE_PATHS=('src/callprog/toneiir.c',);driver.variants=variants;driver.main()
