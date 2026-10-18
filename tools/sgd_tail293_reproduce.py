#!/usr/bin/env python3
"""Bounded normal local-order / multiplication-order cross for SGD tail."""
import itertools
import playbook_small_patterns as d

def variants(path, source):
 a,z,fn=d.function(source,'SGD_correlate')
 cells={'baseline':source}
 original='\tint scale = 1 / (*p * n);\n\tshort acc = 0;\n\tshort i;'
 assert fn.count(original)==1
 for order in itertools.permutations(('scale','acc','i')):
  for swapped in (False,True):
   f=fn.replace('for (i = (short)(n - 1); i != -1; i = (short)(i - 1))','for (i = n; i-- != 0;)').replace('*b++ ^ *a++','*a++ ^ *b++')
   statements={'scale':'\tshort scale = 1 / (*p * n);','acc':'\tshort acc = 0;','i':'\tshort i;'}
   f=f.replace(original,'\n'.join(statements[k] for k in order))
   if swapped:f=f.replace('acc * scale','scale * acc')
   cells['-'.join(order)+('-reverse-product' if swapped else '')]=source[:a]+f+source[z:]
 assert len(cells)==len(set(cells.values()))==13
 return cells

d.REV='96ce7c20d';d.OUT_NAME='sgd-tail293';d.SOURCE_PATHS=('src/fax/Sgd.c',);d.variants=variants;d.main()
