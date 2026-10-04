#!/usr/bin/env python3
import itertools,sys
from pathlib import Path
import playbook_small_patterns as d
import gcc3_batch100_v8_message_capture_reproduce as prior
def variants(path,source):
 s=prior.variants(path,source)['early-1-capture-1'];a,z,fn=d.function(s,'V8GetMessage');cells={'baseline':source}
 for owner,arm in itertools.product([False,True],repeat=2):
  f=fn
  if owner:f=f.replace('if (n > *count)', 'if ((short)received > *count)').replace('rc = n;', 'rc = (short)received;')
  result=s[:a]+f+s[z:]
  if arm:
   result=result.replace('if (v->side != 0)\n\t\treturn &v->seq[0];\n\treturn &v->seq[2];','if (v->side == 0)\n\t\treturn &v->seq[2];\n\treturn &v->seq[0];')
  cells['owner-%d-arm-%d'%(owner,arm)]=result
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='856c1ecb';d.OUT_NAME='gcc3-batch100-v8-message-owner';d.SOURCE_PATHS=('src/v8/V8Interface.c',);d.variants=variants;d.main()
