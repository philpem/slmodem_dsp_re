#!/usr/bin/env python3
"""Verbatim minimal production recipe versus original raw baseline headers."""
import sys,subprocess
from pathlib import Path
import playbook_small_patterns as d

def overlays(path,label):
 if label!='baseline':return {'dsplib/v8.h':(d.ROOT/'include/dsplib/v8.h').read_text()}
 text=subprocess.check_output(['git','show','902f47fa:include/dsplib/v8.h'],cwd=d.ROOT,text=True)
 return {'dsplib/v8.h':text}
def variants(path,source):return {'baseline':source,'production':(d.ROOT/path).read_text()}
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='902f47fa';d.OUT_NAME='gcc3-batch50-integer-production';d.SOURCE_PATHS=('src/pump/v34/v34filters.c','src/v8/V8global.c','src/v8/V8Detector.c','src/v8/V8.c','src/v8/V8Dftc.c','src/v8/V8Fsk.c');d.HEADER_OVERLAYS=overlays;d.variants=variants;d.main()
