#!/usr/bin/env python3
"""Recover unframing parent-owner reads across unknown CRC/debug calls."""
import playbook_small_patterns as d

def variants(path,source):
    start,end,fn=d.function(source,'faxvmi_hdlc_unframe')
    body=fn.index('\n\twhile (left != 0)')
    head=fn[:body];tail=fn[body:]
    assert tail.count('fr->')>=25
    # Keep original entry scalar snapshots, read current parent for later accesses.
    tail=tail.replace('fr->','vmi->framer->')
    return {'baseline':source,'current-parent-reads':source[:start]+head+tail+source[end:]}
if __name__=='__main__':
    d.REV='e0052eec';d.SOURCE_PATHS=('src/fax/faxvmi_hdlc.c',);d.OUT_NAME='fax-unframe-publication'
    d.DUMP_FLAGS=('-v','-save-temps','-da','-dP');d.variants=variants;d.main()
