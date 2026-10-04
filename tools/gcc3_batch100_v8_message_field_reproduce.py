#!/usr/bin/env python3
import sys
from pathlib import Path
import playbook_small_patterns as d
import gcc3_batch100_v8_message_capture_reproduce as prior
def variants(path,source):
 s=prior.variants(path,source)['early-1-capture-1'];a,z,f=d.function(s,'V8GetMessage')
 f=f.replace('\tunsigned short received = seq->wordidx;\n','').replace('(short)received','seq->wordidx')
 s=s[:a]+f+s[z:]
 s=s.replace('if (v->side != 0)\n\t\treturn &v->seq[0];\n\treturn &v->seq[2];','if (v->side == 0)\n\t\treturn &v->seq[2];\n\treturn &v->seq[0];')
 return {'baseline':source,'guarded-field':s}
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='856c1ecb';d.OUT_NAME='gcc3-batch100-v8-message-field';d.SOURCE_PATHS=('src/v8/V8Interface.c',);d.variants=variants;d.main()
