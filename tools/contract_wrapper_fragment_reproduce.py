#!/usr/bin/env python3
"""Test independently observed unsigned fragment comparisons in wrapper state."""
import subprocess
import playbook_small_patterns as d
import contract_wrapper_unsigned_reproduce as prior

def variants(path, source):
    candidate=prior.variants(path,source)['unsigned-1-reload-1']
    return {'baseline':source,'unsigned-reload-control':candidate,'unsigned-fragment':candidate}

def overlay(path,label):
    if label!='unsigned-fragment':return {}
    h=subprocess.check_output(['git','show',d.REV+':include/dsplib/dp_wrapper.h'],cwd=d.ROOT,text=True)
    assert h.count('\tint host_frag;')==1
    return {'dsplib/dp_wrapper.h':h.replace('\tint host_frag;','\tunsigned int host_frag;')}

if __name__=='__main__':
    d.REV='e0052eec';d.SOURCE_PATHS=('src/core/dp_wrapper.c',)
    d.OUT_NAME='contract-wrapper-fragment';d.DUMP_FLAGS=('-v','-save-temps','-da','-dP')
    d.variants=variants;d.HEADER_OVERLAYS=overlay;d.main()
