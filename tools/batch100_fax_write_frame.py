#!/usr/bin/env python3
"""Original frame writer open-coded FCS and ring store boundaries."""
from itertools import product
import playbook_small_patterns as d
from batch100_fax_frame_reverse import variants as frame_variants
d.REV='856c1ecb';d.SOURCE_PATHS=('src/fax/faxvmififo.c',);d.OUT_NAME='batch100-fax-write-frame'
def variants(path,s):
 a,z,f=d.function(s,'faxvmi_write_frame');cells={}
 current=frame_variants(path,s)['literal-unsigned-stride-outer-postdec'];fa,fz,ff=d.function(current,'faxvmi_frame_reverse');sa,sz,_=d.function(s,'faxvmi_frame_reverse')
 literal='''{
			unsigned short *q = p;
			short k;
			fcs = 0xffff;
			for (k = len; k-- != 0;) {
				unsigned int octet = *q++;
				unsigned int t = ((octet << 8) ^ fcs) & 0xf000u;
				fcs = (unsigned short)((((fcs ^ (t >> 11)) << 4) ^ t));
				fcs |= t >> 12;
				t = ((octet << 12) ^ fcs) & 0xf000u;
				fcs = (unsigned short)((((fcs ^ (t >> 11)) << 4) ^ t));
				fcs |= t >> 12;
			}
			fcs = (unsigned short)~fcs;
		}'''
 for body,loops,index in product((False,True),repeat=3):
  x=f
  if body:x=x.replace('fcs = (unsigned short)faxvmi_gen_fcs16(p, len);',literal)
  if loops:x=x.replace('for (i = count; i != 0; i--)','for (i = count; i-- != 0;)').replace('for (j = len; j != 0; j--)','for (j = len; j-- != 0;)')
  if index:
   for expr in ('(unsigned short)(len + 2)','*p++','(unsigned short)(fcs >> 8)','(unsigned short)(fcs & 0xff)'):
    old='fr->fifo[fr->wr] = '+expr+';\n'
    assert old in x
    # whitespace depends on loop nesting; remove precisely following index update.
    import re
    x=re.sub(re.escape(old)+r'\s*fr->wr = \(unsigned short\)\(fr->wr \+ 1\);', 'fr->fifo[fr->wr++] = '+expr+';',x)
  label='-'.join(n for n,v in [('literal-fcs',body),('postdec-loops',loops),('index-postfix',index)] if v) or 'baseline'
  cell=s[:a]+x+s[z:]
  if label!='baseline':cell=cell[:sa]+ff+cell[sz:]
  cells[label]=cell
 return cells
d.variants=variants
if __name__=='__main__':d.main()
