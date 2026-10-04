#!/usr/bin/env python3
import playbook_small_patterns as d
d.REV='856c1ecb';d.SOURCE_PATHS=('src/service/voice.c',);d.OUT_NAME='gcc3-batch100-voice-process-output-capture'
def variants(path,source):
 cells={'baseline':source}
 for frame,length,label in [(True,False,'early-output-frame'),(False,True,'early-output-length'),(True,True,'early-frame-and-length')]:
  start,end,fn=d.function(source,'VOICE_process');anchor='\t\t\t\tRcFixed_Resample(v->rc_in,';assert fn.count(anchor)==1;insert=''
  if frame:
   old='&v->out_ring.data[v->out_ring.blk],';assert fn.count(old)==1;fn=fn.replace(old,'output_frame,');insert+='\t\t\t\tshort *output_frame =\n\t\t\t\t    &v->out_ring.data[v->out_ring.blk];\n'
  if length:
   old='\t\t\t\toutlen = (int)v->block;\n';assert fn.count(old)==1;fn=fn.replace(old,'');insert+='\t\t\t\toutlen = (int)v->block;\n'
  fn=fn.replace(anchor,insert+'\n'+anchor)
  cells[label]=source[:start]+fn+source[end:]
 return cells
d.variants=variants
if __name__=='__main__':d.main()
