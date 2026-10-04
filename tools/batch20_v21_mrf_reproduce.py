#!/usr/bin/env python3
"""Four-cell completed-V21 source boundary / consistent MRF formal cross."""
import playbook_small_patterns as d
import batch20_mrf_formal_reproduce as formal
d.REV='93d7eee1';d.SOURCE_PATHS=('src/fax/V21t_int.c',);d.OUT_NAME='batch20-v21-mrf'
def variants(path,source):
    assert source.count('(short)nsamples')==2
    recovered=source.replace('(short)nsamples','nsamples')
    return {'baseline':source,'count':recovered,'int':source,'int-count':recovered}
def overlays(path,label):return formal.overlays(path,'int' if label.startswith('int') else 'baseline')
d.HEADER_OVERLAYS=overlays;d.variants=variants
if __name__=='__main__':d.main()
