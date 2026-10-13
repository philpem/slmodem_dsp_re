#!/usr/bin/env python3
"""Cross direct AGC results with the recovered V23 diagnostic context."""
import subprocess
import playbook_small_patterns as d
from playbook_agc_unused_argument import SOURCE_PATHS

LABELS=('baseline','return-only','bw-direct','rx-direct','both-direct')


def variants(path,source):
    cells={}
    for label in LABELS:
        text=source
        if label!='baseline' and path=='src/dsp/fpm_agc.c':
            assert text.count('void\nFPM_AGC_agc(')==1
            text=text.replace('void\nFPM_AGC_agc(', 'int\nFPM_AGC_agc(')
            old='\tagc->signal = adjusted > (int)(blocks >> 1);\n}'
            assert text.count(old)==1
            text=text.replace(old,old.replace('\n}', '\n\treturn agc->signal;\n}'))
        if path=='src/pump/v23/bwchdem.c' and label in ('bw-direct','both-direct'):
            old='\tFPM_AGC_agc(&bw->agc, samples, (unsigned short)count, 1);\n\tsignal = bw->agc.signal;'
            assert text.count(old)==1
            text=text.replace(old,'\tsignal = (short)FPM_AGC_agc(&bw->agc, samples, (unsigned short)count, 1);')
        if path=='src/pump/v23/v23rx.c' and label in ('rx-direct','both-direct'):
            old='\tFPM_AGC_agc(&rx->agc, samples, (unsigned short)nout, (short)count);\n\tsignal = rx->agc.signal;'
            assert text.count(old)==1
            text=text.replace(old,'\tsignal = (short)FPM_AGC_agc(&rx->agc, samples, (unsigned short)nout, (short)count);')
        cells[label]=text
    return cells


def overlays(path,label):
    if label=='baseline':return {}
    text=subprocess.check_output(['git','show','ad83fa7c:include/dsplib/fpm_agc.h'],cwd=d.ROOT,text=True)
    assert text.count('void FPM_AGC_agc(')==1
    return {'dsplib/fpm_agc.h':text.replace('void FPM_AGC_agc(', 'int FPM_AGC_agc(')}


if __name__=='__main__':
    d.REV='ad83fa7c'
    d.SOURCE_PATHS=SOURCE_PATHS
    d.OUT_NAME='gcc3-v23-agc-return'
    d.DUMP_FLAGS=('-v','-save-temps','-da')
    d.variants=variants
    d.HEADER_OVERLAYS=overlays
    for path in SOURCE_PATHS:
        variants(path,subprocess.check_output(['git','show',d.REV+':'+path],cwd=d.ROOT,text=True))
    d.main()
