#!/usr/bin/env python3
"""Distinct original FILE frame reverser literal body and length stride."""
from itertools import product
import playbook_small_patterns as d
d.REV='856c1ecb';d.SOURCE_PATHS=('src/fax/faxvmififo.c',);d.OUT_NAME='batch100-fax-frame-reverse'
def variants(path,source):
 a,z,fn=d.function(source,'faxvmi_frame_reverse');cells={}
 literal='''{
			unsigned short *p = buf;
			short n;

			for (n = (short)len; n-- != 0;) {
				unsigned short in = *p, out = 0;
				unsigned short bit;

				for (bit = 8; bit-- != 0;) {
					out = (unsigned short)(out << 1);
					out |= in & 1;
					in >>= 1;
				}
				*p++ = out;
			}
		}'''
 for body,unsigned,outer in product((False,True),repeat=3):
  x=fn
  if body:x=x.replace('faxvmi_byte_reverse(buf, len);',literal)
  if unsigned:x=x.replace('short len = (short)*buf++;','unsigned short len = *buf++;').replace('faxvmi_byte_reverse(buf, len);','faxvmi_byte_reverse(buf, (short)len);')
  if outer:x=x.replace('for (i = count; i != 0; i--)','for (i = count; i-- != 0;)')
  label='-'.join(n for n,v in [('literal',body),('unsigned-stride',unsigned),('outer-postdec',outer)] if v) or 'baseline'
  cells[label]=source[:a]+x+source[z:]
 return cells
d.variants=variants
if __name__=='__main__':d.main()
