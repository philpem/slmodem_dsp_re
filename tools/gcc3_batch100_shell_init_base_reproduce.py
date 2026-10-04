#!/usr/bin/env python3
import sys
from pathlib import Path
import playbook_small_patterns as d
import gcc3_batch100_shell_init_reproduce as prior
def variants(path,source):
 s=prior.variants(path,source)['word-1-early-1-index-1'];a,z,f=d.function(s,'initG248')
 f=f.replace('unsigned top = 2 * (n - 1);','unsigned base = n - 1;\n\tunsigned top = 2 * base;').replace('s->t2[2 * top - j]','s->t2[4 * base - j]')
 return {'baseline':source,'shared-base':s[:a]+f+s[z:]}
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='856c1ecb';d.OUT_NAME='gcc3-batch100-shell-init-base';d.SOURCE_PATHS=('src/pump/v34/v34shell.c',);d.variants=variants;d.main()
