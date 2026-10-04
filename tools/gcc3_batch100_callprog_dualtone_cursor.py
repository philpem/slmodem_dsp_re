#!/usr/bin/env python3
import itertools
import playbook_small_patterns as d
d.REV='856c1ecb';d.SOURCE_PATHS=('src/callprog/DualTone_Detector.c',);d.OUT_NAME='gcc3-batch100-callprog-dualtone-cursor'
def variants(path,source):
 cells={}
 for loop,owned in itertools.product((False,True),repeat=2):
  start,end,fn=d.function(source,'Dual_TONE_detect')
  if owned:
   fn=fn.replace('{\n','{\n\tshort *bp = st->bp;\n\tshort *na = st->notch_a;\n\tshort *nb = st->notch_b;\n\tshort *nc = st->notch_c;\n',1)
   fn=fn.replace('samples[i]','*samples').replace('\n\t\tya = notch(st->notch_a,','\n\t\tsamples++;\n\t\tya = notch(na,').replace('notch(st->notch_b,','notch(nb,').replace('notch(st->notch_c,','notch(nc,').replace('&st->bp[4 * s]','&bp[4 * s]')
  if loop:
   fn=fn.replace('\t\tint s;','\t\tshort s;\n\t\tshort *h = '+('bp' if owned else 'st->bp')+';\n\t\tconst short *coef = IIRFilterCoef;')
   old='\t\tfor (s = 0; s < DUAL_TONE_BP_SECTIONS; s++) {\n\t\t\tx = bp_section('+('&bp[4 * s]' if owned else '&st->bp[4 * s]')+', &IIRFilterCoef[5 * s],\n\t\t\t\t       x);\n\t\t\tx = (short)((int)x >> IIRFilterScales[s + 1]);\n\t\t}'
   new='\t\ts = DUAL_TONE_BP_SECTIONS - 1;\n\t\tdo {\n\t\t\tx = bp_section(h, coef, x);\n\t\t\tx = (short)((int)x >> IIRFilterScales[DUAL_TONE_BP_SECTIONS - s]);\n\t\t\th += 4;\n\t\t\tcoef += 5;\n\t\t} while (--s >= 0);'
   assert fn.count(old)==1;fn=fn.replace(old,new)
  label='-'.join(n for n,v in [('short-bp-cursors',loop),('history-sample-ownership',owned)] if v) or 'baseline'
  cells[label]=source[:start]+fn+source[end:]
 return cells
d.variants=variants
if __name__=='__main__':d.main()
