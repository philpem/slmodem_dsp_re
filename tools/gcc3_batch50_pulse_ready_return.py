#!/usr/bin/env python3
"""Original shared count verdict after the observed callback-reload graph."""
import playbook_small_patterns as d
from gcc3_batch50_pulse_ready import variants as prior

def variants(path,source):
 cells={'baseline':source}
 text=prior(path,source)['unsigned-1-reload-1-common-1']
 start,end,fn=d.function(text,'IsPulseDialerReady')
 old='\tif (remaining == 0)\n\t\treturn 1;'
 assert old in fn;fn=fn.replace(old,'\tif (remaining != 0) {')
 fn=fn.replace('\treturn c->pulse_remaining == 0;','\t}\n\treturn c->pulse_remaining == 0;')
 cells['shared-count-verdict']=text[:start]+fn+text[end:]
 return cells
if __name__=='__main__':
 d.REV='902f47fa';d.SOURCE_PATHS=('src/call/call.c',);d.OUT_NAME='gcc3-batch50-pulse-ready-return';d.variants=variants;d.main()
