#!/usr/bin/env python3
import itertools,sys
from pathlib import Path
import playbook_small_patterns as d
import gcc3_batch100_v8_message_reproduce as prior
def variants(path,source):
 s=prior.variants(path,source)['guard-1-unsigned-1'];a,z,fn=d.function(s,'V8GetMessage');cells={'baseline':source}
 for early,capture in itertools.product([False,True],repeat=2):
  f=fn
  if early:
   f=f.replace('\tint rc = V8_GET_EMPTY;\n','')
   f=f.replace('\tconst struct v8_tx_sequence *seq', '\tint rc = V8_GET_EMPTY;\n\tconst struct v8_tx_sequence *seq')
  if capture:
   f=f.replace('short received = seq->wordidx;','unsigned short received = seq->wordidx;').replace('if (received > 0)','if ((short)received > 0)').replace('int n = received;','int n = (short)received;')
  cells['early-%d-capture-%d'%(early,capture)]=s[:a]+f+s[z:]
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='856c1ecb';d.OUT_NAME='gcc3-batch100-v8-message-capture';d.SOURCE_PATHS=('src/v8/V8Interface.c',);d.variants=variants;d.main()
