#!/usr/bin/env python3
"""IIR output ownership discriminator, crossed with established section cube."""
import sys
from pathlib import Path
import playbook_small_patterns as d
import gcc3_batch100_iir_sections_reproduce as prior
def variants(path,source):
 cells={'baseline':source}
 for name,s in prior.variants(path,source).items():
  a,z,f=d.function(s,'FPM_iir_filt')
  f=f.replace('int acc_in = x;','int acc_in = x;\n\tint output;')
  f=f.replace('acc_in = (short)((short)ff + ((', 'output = (short)((short)ff + ((')
  f=f.replace(' >> 14));\n', ' >> 14));\n\t\tacc_in = output;\n')
  assert f.count('output = (short)') == 1 and f.count('acc_in = output;') == 1
  f=f.replace('return (short)acc_in;','return (short)output;')
  cells['output-'+name]=s[:a]+f+s[z:]
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='856c1ecb';d.OUT_NAME='gcc3-batch100-iir-result';d.SOURCE_PATHS=('src/dsp/fpm_iir.c',);d.variants=variants;d.main()
