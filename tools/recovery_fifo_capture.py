#!/usr/bin/env python3
"""Cross witnessed Class1 FIFO result captures without a prototype change."""
import sys
import playbook_small_patterns as d

def variants(path, source):
    result={}
    for reads in (False, True):
        for write in (False, True):
            text=source
            if reads:
                old="*word8 = FIFO_read("
                assert text.count(old)==1
                text=text.replace(old,"*word8 = (unsigned short)FIFO_read(")
                old="rd = FIFO_read("
                assert text.count(old)==1
                text=text.replace(old,"rd = (unsigned short)FIFO_read(")
            if write:
                old="n = FIFO_write("
                assert text.count(old)==1
                text=text.replace(old,"n = (unsigned short)FIFO_write(")
            label="baseline" if not reads and not write else "reads-%d-write-%d"%(reads,write)
            result[label]=text
    assert len(result)==len(set(result.values()))==4
    return result

if __name__=="__main__":
    assert "--domain" in sys.argv
    d.REV="4169cb3b"
    d.OUT_NAME="recovery-fifo-capture"
    d.SOURCE_PATHS=("src/fax/cDATAtx.c",)
    d.DUMP_FLAGS=("-v","-save-temps","-dr","-dc")
    d.variants=variants
    d.main()
