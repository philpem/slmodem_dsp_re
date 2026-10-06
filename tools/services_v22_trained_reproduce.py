#!/usr/bin/env python3
"""Recover V22 trained-symbol loop ownership from original complete bodies."""
import itertools
import playbook_small_patterns as d

def replace(t,name,body):
 a,b,old=d.function(t,name);return t[:a]+name+old[old.index('('):old.index('{')]+body+t[b:]

def variants(path,source):
 cells={'baseline':source}
 f1200='''{
	unsigned short n = *count;
	short i;
	for (i = 0; i < (int)n && symbols[i] == V22_TRAINED_1200_SYMBOL; i++)
		;
	return i == (int)n;
}'''
 f2400='''{
	unsigned short n = *count;
	short run = 0;
	short i = (short)(n - 1);
	while (run < (int)n && symbols[i--] == V22_TRAINED_2400_SYMBOL)
		run++;
	return run > V22_TRAINED_2400_RUN;
}'''
 for first,second,branch in itertools.product((False,True),repeat=3):
  if not(first or second or branch):continue
  t=source
  if first:t=replace(t,'RxTrained1200',f1200)
  if second:t=replace(t,'RxTrained2400',f2400)
  if branch:
   a,b,body=d.function(t,'RxTrained2400');needle='\treturn run > V22_TRAINED_2400_RUN;';assert needle in body
   body=body.replace(needle,'\tif (run > V22_TRAINED_2400_RUN)\n\t\treturn 1;\n\treturn 0;');t=t[:a]+body+t[b:]
  cells[f'forward-{int(first)}-backward-{int(second)}-branch-{int(branch)}']=t
 return cells
if __name__=='__main__':
 d.REV='e0052eec';d.SOURCE_PATHS=('src/pump/v22/v22prc.c',);d.OUT_NAME='services-v22-trained'
 d.DUMP_FLAGS=('-v','-save-temps','-da','-dP');d.variants=variants;d.main()
