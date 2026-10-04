#!/usr/bin/env python3
import playbook_small_patterns as d
d.REV='856c1ecb';d.SOURCE_PATHS=('src/service/voice.c',);d.OUT_NAME='gcc3-batch100-voice-process-scale-operand'
def variants(path,source):
 cells={'baseline':source}
 for scale,owner,label in [(True,False,'output-scale-first'),(False,True,'early-owners-control'),(True,True,'scale-first-early-owners')]:
  start,end,fn=d.function(source,'VOICE_process')
  if scale:
   old='v->to_line[i]\n\t\t\t\t\t\t    * VCE_LINE_OUT_SCALE';assert fn.count(old)==1;fn=fn.replace(old,'VCE_LINE_OUT_SCALE\n\t\t\t\t\t\t    * v->to_line[i]')
  if owner:
   old='&v->out_ring.data[v->out_ring.blk],';assert fn.count(old)==1;fn=fn.replace(old,'output_frame,')
   old='\t\t\t\toutlen = (int)v->block;\n';assert fn.count(old)==1;fn=fn.replace(old,'')
   anchor='\t\t\t\tRcFixed_Resample(v->rc_in,';assert fn.count(anchor)==1;fn=fn.replace(anchor,'\t\t\t\tshort *output_frame =\n\t\t\t\t    &v->out_ring.data[v->out_ring.blk];\n\t\t\t\toutlen = (int)v->block;\n\n'+anchor)
  cells[label]=source[:start]+fn+source[end:]
 return cells
d.variants=variants
if __name__=='__main__':d.main()
