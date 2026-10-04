#!/usr/bin/env python3
"""Trace original online status assignment relative to diagnostic/kernel calls."""
import playbook_small_patterns as d
def variants(path,source):
 cells={'baseline':source};start,end,fn=d.function(source,'voice_online')
 marker='\tif (r == 1) {\n';old='\t\tret = 1;\n';assert fn.count(old)==1
 for label,target in [('before-diagnostic',marker),('before-store','\t\tv->beep_done = 1;\n'),('before-kernel','\t\tFDSP_Kernel_SetInternalBeepInProgress(0);\n')]:
  body=fn.replace(old,'')
  if label=='before-diagnostic':body=body.replace(target,target+old)
  else:body=body.replace(target,old+target)
  cells[label]=source[:start]+body+source[end:]
 return cells
if __name__=='__main__':
 d.REV='856c1ecb';d.SOURCE_PATHS=('src/voice/voice.c',);d.OUT_NAME='gcc3-batch100-voice-online';d.variants=variants;d.main()
