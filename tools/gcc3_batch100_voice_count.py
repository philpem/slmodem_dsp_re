#!/usr/bin/env python3
"""Cross original full-width caller word with callee signed-short use."""
import playbook_small_patterns as d
LABELS=('baseline','wide-int','caller-only','wide-int-plain','wide-uint-plain')
def variants(path,source):
 cells={}
 for label in LABELS:
  body=source
  if path.endswith('/Detector.c') and label.startswith('wide-'):
   typ='unsigned int' if label=='wide-uint-plain' else 'int'
   old='detector_progress(struct detector *d, float *samples, short count,\n\t\t  unsigned char *out, unsigned short *outlen)\n{'
   new=old.replace('short count',typ+' incoming_count')+'\n\tshort count = (short)incoming_count;'
   assert old in body;body=body.replace(old,new)
  if path.endswith('/voice.c') and (label=='caller-only' or label.endswith('-plain')):
   old='detector_progress(v->detector, tx_flt, (short)*countp, out, &det_len)'
   assert old in body;body=body.replace(old,old.replace('(short)*countp','*countp'))
  cells[label]=body
 return cells
def headers(path,label):
 if not label.startswith('wide-'):return {}
 p='include/dsplib/detector.h';text=(d.ROOT/p).read_text();old='int detector_progress(struct detector *d, float *samples, short count,'
 typ='unsigned int' if label=='wide-uint-plain' else 'int';assert old in text
 return {'dsplib/detector.h':text.replace(old,old.replace('short count',typ+' incoming_count'))}
if __name__=='__main__':
 d.REV='856c1ecb';d.SOURCE_PATHS=('src/service/Detector.c','src/voice/voice.c');d.OUT_NAME='gcc3-batch100-voice-count';d.variants=variants;d.HEADER_OVERLAYS=headers;d.main()
