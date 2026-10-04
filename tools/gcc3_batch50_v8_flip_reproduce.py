#!/usr/bin/env python3
"""Byte nibble reversal with an unsigned full-register input carrier."""
import sys,re
from pathlib import Path
import playbook_small_patterns as d

def variants(path,source):
 a,z,fn=d.function(source,'charFlip')
 body=fn.replace('{\n','{\n\tunsigned bits = b;\n',1).replace('wordFlip[b &','wordFlip[bits &').replace('wordFlip[b >>','wordFlip[bits >>')
 assert body!=fn
 return {'baseline':source,'promoted-input':source[:a]+body+source[z:]}
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='902f47fa';d.OUT_NAME='gcc3-batch50-v8-flip';d.SOURCE_PATHS=('src/v8/V8global.c',);d.variants=variants;d.main()
