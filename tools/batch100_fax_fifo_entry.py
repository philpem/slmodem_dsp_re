#!/usr/bin/env python3
"""One-time framer capture after original nonzero gate."""
import playbook_small_patterns as d
from batch100_fax_ring_writers import variants as writer_variants
d.REV='856c1ecb';d.SOURCE_PATHS=('src/fax/faxvmififo.c',);d.OUT_NAME='batch100-fax-fifo-entry'
def variants(path,source):
 c=writer_variants(path,source)['postdec-index-postfix-late-framer']
 a,z,fn=d.function(c,'faxvmi_write_fifo')
 fn=fn.replace('for (i = count; i-- != 0;) {\n\t\tfr = vmi->framer;', 'i = count;\n\tif (i-- != 0) {\n\t\tfr = vmi->framer;\n\t\tdo {')
 fn=fn.replace('\t\ttaken = 0;\n\t}', '\t\ttaken = 0;\n\t\t} while (i-- != 0);\n\t}')
 return {'baseline':source,'guarded-once':c[:a]+fn+c[z:]}
d.variants=variants
if __name__=='__main__':d.main()
