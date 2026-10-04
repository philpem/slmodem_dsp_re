#!/usr/bin/env python3
"""Original V8 SetMessage selector/result-lifetime boundaries, eight cells."""
import itertools
import playbook_small_patterns as d
d.REV='93d7eee1';d.SOURCE_PATHS=('src/v8/V8Interface.c',);d.OUT_NAME='batch20-v8-set'
def variants(path,source):
 out={}
 for direct,unsigned,late in itertools.product((False,True),repeat=3):
  text=source;a,b,fn=d.function(text,'V8SetMessage')
  if direct:
   fn=fn.replace('struct v8_tx_sequence *seq = selected_sequence(v, which);','struct v8_tx_sequence *seq;')
   start=fn.index('\tif (seq == 0) {');end=fn.index('\n\t}',start)+3
   old=fn[start:end];default=old.split('\n',1)[1].rsplit('\n',1)[0]
   switch='\tswitch ('+('(unsigned)which' if unsigned else 'which')+') {\n\tcase V8_SET_CM: seq = &v->seq[0]; break;\n\tcase V8_SET_JM: seq = &v->seq[2]; break;\n\tcase V8_SET_CJ: seq = &v->seq[1]; break;\n\tcase V8_SET_QC1A: seq = &v->seq[3]; break;\n\tdefault:\n'+default+'\n\t}'
   fn=fn[:start]+switch+fn[end:]
  if late:
   fn=fn.replace('int rc = 0;','int rc;');fn=fn.replace('\tif (n > V8_TX_SEQ_WORDS)','\trc = 0;\n\n\tif (n > V8_TX_SEQ_WORDS)')
  text=text[:a]+fn+text[b:]
  if unsigned and not direct:
   a,b,f=d.function(text,'selected_sequence');f=f.replace('switch (which)','switch ((unsigned)which)');text=text[:a]+f+text[b:]
  label='baseline' if not(direct or unsigned or late) else ('direct' if direct else 'helper')+('-unsigned' if unsigned else '')+('-late' if late else '')
  out[label]=text
 assert len(out)==len(set(out.values()))==8
 return out
d.variants=variants
if __name__=='__main__':d.main()
