#!/usr/bin/env python3
"""Original signed input carrier and shortcut conversion controls."""
import sys
from pathlib import Path
import playbook_small_patterns as d

def variants(path,source):
 a,z,fn=d.function(source,'FPM_atan')
 promoted=fn.replace('FPM_atan(short y, short x, short *angle)','FPM_atan(short y_arg, short x_arg, short *angle)').replace('{\n','{\n\tint y = y_arg;\n\tint x = x_arg;\n',1)
 narrow=promoted.replace('? (int)ratio :','? (int)(short)ratio :')
 assert narrow!=promoted!=fn
 return {'baseline':source,'promoted-inputs':source[:a]+promoted+source[z:],'promoted-shortcut':source[:a]+narrow+source[z:]}
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='902f47fa';d.OUT_NAME='gcc3-batch50-atan-inputs';d.SOURCE_PATHS=('src/dsp/fpm_atan.c',);d.variants=variants;d.main()
