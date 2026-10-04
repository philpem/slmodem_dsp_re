#!/usr/bin/env python3
"""Cross original eager two-predicate gate with next-state call lifetime."""
import itertools
import playbook_small_patterns as d
def variants(path,source):
 cells={};start,end,fn=d.function(source,'voice_online')
 for eager,early in itertools.product((False,True),repeat=2):
  body=fn;label='baseline' if not(eager or early) else 'eager-%d-early-%d'%(eager,early)
  if eager:
   old='while (i < *countp && r != 1)';assert body.count(old)==2
   body=body.replace(old,'while ((i < *countp) & (r != 1))')
  if early:
   old='\t\tret = 1;\n';assert body.count(old)==1
   marker='\t\tFDSP_Kernel_SetInternalBeepInProgress(0);\n'
   body=body.replace(old,'').replace(marker,old+marker)
  cells[label]=source[:start]+body+source[end:]
 return cells
if __name__=='__main__':
 d.REV='856c1ecb';d.SOURCE_PATHS=('src/voice/voice.c',);d.OUT_NAME='gcc3-batch100-voice-online-gate';d.variants=variants;d.main()
