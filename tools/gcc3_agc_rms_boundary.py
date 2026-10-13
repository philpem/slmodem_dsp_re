#!/usr/bin/env python3
"""Cross RMS formal/countdown boundary with full AGC chosen-block length."""
import subprocess
import playbook_small_patterns as d
from gcc3_agc_length import variants as previous

SOURCE_PATHS=('src/dsp/fpm_rms.c','src/dsp/fpm_agc.c','src/pump/v32/V32int.c',
              'src/pump/v22/V22int.c','src/pump/v22/v22mod.c','src/dsp/fpm_sre.c',
              'src/fax/V27r_int.c','src/fax/V29r_int.c','src/fax/class1.c','src/fax/V17r_int.c')


def variants(path,source):
    cells={}
    for wide in (False,True):
        for length in (False,True):
            label={ (False,False):'baseline',(True,False):'wide',
                    (False,True):'length',(True,True):'both'}[wide,length]
            text=source
            if path=='src/dsp/fpm_agc.c':
                text=previous(path,source)['full-length' if length else 'baseline']
            if path=='src/dsp/fpm_rms.c' and wide:
                old='FPM_rms(const short *samples, unsigned short count)\n{\n\tint sum = 0;'
                assert text.count(old)==1
                text=text.replace(old,'FPM_rms(const short *samples, unsigned length)\n{\n\tunsigned short count = (unsigned short)length;\n\tint sum = 0;')
            cells[label]=text
    return cells


def headers(path,label):
    if label in ('baseline','length'):return {}
    text=subprocess.check_output(['git','show','6eaeaedd:include/dsplib/fpm.h'],cwd=d.ROOT,text=True)
    old='short FPM_rms(const short *samples, unsigned short count);'
    assert text.count(old)==1
    return {'dsplib/fpm.h':text.replace(old,'short FPM_rms(const short *samples, unsigned length);')}


if __name__=='__main__':
    d.REV='6eaeaedd'
    d.SOURCE_PATHS=SOURCE_PATHS
    d.OUT_NAME='gcc3-agc-rms-boundary'
    d.DUMP_FLAGS=('-v','-save-temps','-da')
    d.variants=variants
    d.HEADER_OVERLAYS=headers
    d.main()
