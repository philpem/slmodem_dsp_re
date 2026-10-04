#!/usr/bin/env python3
"""Original literal switch × bank6/7 mapping, complete period TU controls."""
import playbook_small_patterns as d
d.REV='856c1ecb';d.SOURCE_PATHS=('src/callprog/Cadence.c',);d.OUT_NAME='gcc3-batch100-callprog-cadence-banks'
def variants(path,source):
 start,end,fn=d.function(source,'select_filter')
 cells={'baseline':source}
 for switch,correct,label in [(False,True,'table-original-map'),(True,False,'switch-current-map'),(True,True,'switch-original-map')]:
  if not switch:
   body=fn.replace('{ 0, 0, 0, CP_450_630_a', '{ Filter_100_550_a, Filter_100_550_b, Filter_100_550_scales, CP_450_630_a').replace('{ 0, 0, 0, CP_100_550_a', '{ Filter_350_500_a, Filter_350_500_b, Filter_350_500_scales, CP_100_550_a')
  else:
   body='select_filter(struct cadence *c, int index, int sub)\n{\n\tc->sel_n_a = IIR_FILTER_COEFF;\n\tc->sel_n_b = IIR_FILTER_COEFF;\n\tswitch (index) {\n'
   for cases,bank,fallback in [([0,2],'350_500','350_600'),([1,4,5],'100_550','350_600'),([3],'276_504','276_504'),([6],'100_550' if correct else None,'450_630'),([7],'350_500' if correct else None,'100_550')]:
    body+=''.join('\tcase %d:\n'%n for n in cases)
    if bank:
     body+='\t\tif ((unsigned)(sub - 1) <= 6) {\n'
     for field,stride in [('a','IIR_FILTER_COEFF'),('b','IIR_FILTER_COEFF'),('scales','IIR_FILTER_SCALES')]:
      body+='\t\t\tc->sel_%s = Filter_%s_%s + %s * (sub - 1);\n'%(field,bank,field,stride)
     body+='\t\t\tbreak;\n\t\t}\n'
    for field in ('a','b','scales'):body+='\t\tc->sel_%s = CP_%s_%s;\n'%(field,fallback,field)
    body+='\t\tbreak;\n'
   body+='\tdefault:\n'
   for field in ('a','b','scales'):body+='\t\tc->sel_%s = CP_350_600_%s;\n'%(field,field)
   body+='\t\tbreak;\n\t}\n}'
  text=source[:start]+body+source[end:]
  if switch:
   a=text.index('struct cadence_bank {');z=text.index('\n};',a)+3;text=text[:a]+text[z:]
  cells[label]=text
 return cells
d.variants=variants
if __name__=='__main__':d.main()
