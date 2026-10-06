#!/usr/bin/env python3
"""Test the original post-callback framer capture."""
import playbook_small_patterns as d

def variants(path,source):
    start,end,fn=d.function(source,'FAXVMI_process')
    old='\tstruct faxvmi_framer *fr = vmi->framer;'
    assert fn.count(old)==1
    fn=fn.replace(old,'\tstruct faxvmi_framer *fr;')
    mark='\tif (fr->fifo_size - fr->count < vmi->max_frame)'
    assert fn.count(mark)==1
    fn=fn.replace(mark,'\tfr = vmi->framer;\n'+mark)
    return {'baseline':source,'post-callback-framer':source[:start]+fn+source[end:]}
if __name__=='__main__':
    d.REV='e0052eec';d.SOURCE_PATHS=('src/fax/faxvmi.c',);d.OUT_NAME='fax-framer-publication'
    d.DUMP_FLAGS=('-v','-save-temps','-da','-dP');d.variants=variants;d.main()
