#!/usr/bin/env python3
"""Cross original unsigned verdict and syntax-owner evaluation before debug."""
import itertools
import playbook_small_patterns as d

def variants(path,source):
 cells={'baseline':source};start,end,fn=d.function(source,'IsDialStringInvalid')
 for unsigned,early in itertools.product((False,True),repeat=2):
  if not(unsigned or early):continue
  body=fn
  if unsigned:body=body.replace('int grade;','unsigned int grade;')
  if early:
   body=body.replace('grade = AnalyseDialString(d, s, 0);','grade = AnalyseDialString(d, s, 0);\n\tconst char *syntax = dialer_grade_name(grade);')
   body=body.replace('     dialer_grade_name(grade));','     syntax);')
   assert body.count('dialer_grade_name(grade)')==1
  cells['unsigned-%d-early-%d'%(unsigned,early)]=source[:start]+body+source[end:]
 return cells
if __name__=='__main__':
 d.REV='856c1ecb';d.SOURCE_PATHS=('src/dialer/Dialer.c',);d.OUT_NAME='gcc3-batch100-dialer-verdict';d.variants=variants;d.main()
