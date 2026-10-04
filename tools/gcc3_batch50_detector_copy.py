#!/usr/bin/env python3
"""Cross detector configuration copy boundary with setup source order."""
import playbook_small_patterns as d
def variants(path,source):
 a,z,fn=d.function(source,'detector_create');cells={}
 for staged in (0,1):
  for order in (0,1):
   q=fn
   if staged:
    q=q.replace('\tstruct fdsp_tone_cfg cfg;','\tstruct fdsp_tone_cfg cfg;\n\tstruct dtmf_rx *new_dtmf;')
    q=q.replace('d->dtmf = create_dtmf(d->dtmf);','new_dtmf = create_dtmf(d->dtmf);')
    q=q.replace('cfg = TONEamode_CFG;','cfg = TONEamode_CFG;\n\td->dtmf = new_dtmf;')
   if order:q=q.replace('\ts.tone = CADENCE_TONE_BUSY;\n\ts.w6 = 0;','\ts.w6 = 0;\n\ts.tone = CADENCE_TONE_BUSY;')
   label='baseline' if not(staged or order) else f'staged-{staged}-setup-{order}'
   cells[label]=source[:a]+q+source[z:]
 assert len(set(cells.values()))==4
 return cells
if __name__=='__main__':
 d.REV='902f47fa';d.SOURCE_PATHS=('src/service/Detector.c',);d.OUT_NAME='gcc3-batch50-detector-copy';d.variants=variants;d.main()
