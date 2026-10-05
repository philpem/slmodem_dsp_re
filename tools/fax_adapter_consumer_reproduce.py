#!/usr/bin/env python3
"""Replay all direct/transitive faxadapt header consumers under unified returns."""
import re
import subprocess
import playbook_small_patterns as d
import fax_adapter_result_reproduce as a

def variants(path,source):
    if 'Vmi_v' in path:return a.variants(path,source)
    cells={'baseline':source,'modem-status':source}
    if path=='src/fax/faxvmi.c':
        text=source
        for mode in ['17','21','27','29']:
            for direction in ['rx','tx']:
                old='(faxvmi_process_fn)v'+mode+direction+'_process'
                assert text.count(old)==1
                text=text.replace(old,'v'+mode+direction+'_process')
        cells['typed-table']=text
    return cells

def overlay(path,label):
    if label=='baseline':return {}
    text=subprocess.check_output(['git','show',d.REV+':include/dsplib/faxadapt.h'],cwd=d.ROOT,text=True)
    for mode in ['17','21','27','29']:
        for direction in ['rx','tx']:
            name='v'+mode+direction+'_process'
            text,n=re.subn(r'\bvoid\s+'+name+r'\(', 'int '+name+'(',text)
            assert n==1,(name,n)
    return {'dsplib/faxadapt.h':text}
if __name__=='__main__':
    d.REV='240481e6';d.SOURCE_PATHS=tuple(sorted(str(p.relative_to(d.ROOT)) for p in (d.ROOT/'src').rglob('*.c') if '"dsplib/faxadapt.h"' in p.read_text()))
    assert len(d.SOURCE_PATHS)==10,d.SOURCE_PATHS
    # No other header includes faxadapt, so direct consumers exhaust closure.
    assert not any('"dsplib/faxadapt.h"' in p.read_text() for p in (d.ROOT/'include').rglob('*.h'))
    d.OUT_NAME='fax-adapter-consumers';d.DUMP_FLAGS=('-v','-save-temps','-da','-dP');d.variants=variants;d.HEADER_OVERLAYS=overlay;d.main()
