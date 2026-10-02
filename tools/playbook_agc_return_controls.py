#!/usr/bin/env python3
"""Separate an ordinary AGC int return from observed V21 result consumption."""
import playbook_small_patterns as driver

def variants(path,source):
    if path.endswith('fpm_agc.c'):
        old='void\nFPM_AGC_agc('
        assert source.count(old)==1
        text=source.replace(old,'int\nFPM_AGC_agc(')
        old='\tagc->signal = adjusted > (int)(blocks >> 1);\n}'
        assert text.count(old)==1
        text=text.replace(old,old.replace('\n}','\n\treturn agc->signal;\n}'))
        return {'baseline':source,'return-only':text,'consume':text}
    old='\tFPM_AGC_agc(&rx->dsp->agc, in, count, 1);\n\n\trx->dsp->int_0004 = rx->dsp->agc.signal;'
    assert source.count(old)==1
    text=source.replace(old,'\trx->dsp->int_0004 = FPM_AGC_agc(&rx->dsp->agc, in, count, 1);')
    return {'baseline':source,'return-only':source,'consume':text}

def overlays(path,label):
    if label=='baseline':return {}
    rel='dsplib/fpm_agc.h';text=(driver.ROOT/'include'/rel).read_text()
    old='void FPM_AGC_agc('
    assert text.count(old)==1
    return {rel:text.replace(old,'int FPM_AGC_agc(')}

if __name__=='__main__':
    import sys, subprocess
    assert '--domain' in sys.argv, 'predeclared domain URL required'
    assert sys.argv[sys.argv.index('--domain')+1].startswith('https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-'), 'invalid or empty domain URL'
    driver.REV='9a65b5a7'
    driver.OUT_NAME='playbook-agc-return-controls'
    driver.SOURCE_PATHS=('src/dsp/fpm_agc.c','src/fax/V21r_int.c')
    driver.variants=variants
    driver.HEADER_OVERLAYS=overlays
    driver.main()
