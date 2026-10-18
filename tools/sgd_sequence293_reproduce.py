#!/usr/bin/env python3
"""Four native-witnessed SGD sequence countdown/lifetime complete-TU controls."""
import itertools
import playbook_small_patterns as d

def variants(path,source):
 a,z,fn=d.function(source,'SGD_sequence_gen');cells={}
 old='''		unsigned short sym;

		if (active != 0) {
			sym = seq[i];
			i = (short)(i + 1);
		} else {
			sym = s->cfg.gen.idle_sym;
		}
		*out++ = sym;'''
 assert fn.count(old)==1
 for countdown,expression in itertools.product((False,True),repeat=2):
  f=fn
  if countdown:f=f.replace('for (k = (short)(n - 1); k != -1; k = (short)(k - 1))','for (k = n; k-- != 0;)')
  if expression:f=f.replace(old,'\t\t*out++ = active != 0 ? seq[i++] : s->cfg.gen.idle_sym;')
  label='baseline' if not(countdown or expression) else '-'.join(k for k,v in [('postdecrement',countdown),('expression',expression)] if v)
  cells[label]=source[:a]+f+source[z:]
 assert len(cells)==len(set(cells.values()))==4
 return cells

d.REV='a988129c0';d.OUT_NAME='sgd-sequence293';d.SOURCE_PATHS=('src/fax/Sgd.c',);d.DUMP_FLAGS=('-v','-save-temps','-da');d.variants=variants;d.main()
