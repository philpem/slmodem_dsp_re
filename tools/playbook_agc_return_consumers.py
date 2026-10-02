#!/usr/bin/env python3
"""Extend separated AGC return controls to six observed result consumers."""
import playbook_small_patterns as driver
import playbook_agc_return_controls as minimal
import playbook_agc_unused_argument as fourth

PAIRS={
    'src/pump/v23/v23rx.c': ('\tFPM_AGC_agc(&rx->agc, samples, (unsigned short)nout, (short)count);\n\tsignal = rx->agc.signal;', '\tsignal = (short)FPM_AGC_agc(&rx->agc, samples, (unsigned short)nout, (short)count);'),
    'src/pump/v23/bwchdem.c': ('\tFPM_AGC_agc(&bw->agc, samples, (unsigned short)count, 1);\n\tsignal = bw->agc.signal;', '\tsignal = (short)FPM_AGC_agc(&bw->agc, samples, (unsigned short)count, 1);'),
    'src/fax/V17r_int.c': ('\tFPM_AGC_agc(&RXS(modem)->agc.value, in, count, 1);\n\t/* Not the object\'s `%eax`; the same value.  D1091. */\n\tsignal = RXS(modem)->agc.value.signal;', '\tsignal = FPM_AGC_agc(&RXS(modem)->agc.value, in, count, 1);'),
    'src/fax/V27r_int.c': ('\tFPM_AGC_agc(&((struct v27_rx *)modem)->rx->agc, in, count, 1);\n\t/* Not the object\'s `%eax`; the same value.  D1094. */\n\tsignal = ((struct v27_rx *)modem)->rx->agc.signal;', '\tsignal = FPM_AGC_agc(&((struct v27_rx *)modem)->rx->agc, in, count, 1);'),
    'src/fax/V29r_int.c': ('\tFPM_AGC_agc(&((struct v29_rx *)modem)->rx->agc, in, count, 1);\n\t/* Not the object\'s `%eax`; the same value.  D1036. */\n\tsignal = ((struct v29_rx *)modem)->rx->agc.signal;', '\tsignal = FPM_AGC_agc(&((struct v29_rx *)modem)->rx->agc, in, count, 1);'),
}

def variants(path,source):
    if path in ('src/dsp/fpm_agc.c','src/fax/V21r_int.c'):
        return minimal.variants(path,source)
    text=source
    if path in PAIRS:
        old,new=PAIRS[path]
        assert source.count(old)==1,path
        text=source.replace(old,new)
    return {'baseline':source,'return-only':source,'consume':text}

if __name__=='__main__':
    import sys, subprocess
    assert '--domain' in sys.argv, 'predeclared domain URL required'
    assert sys.argv[sys.argv.index('--domain')+1].startswith('https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-'), 'invalid or empty domain URL'
    driver.REV='9a65b5a7'
    driver.OUT_NAME='playbook-agc-return-consumers'
    driver.SOURCE_PATHS=fourth.SOURCE_PATHS
    driver.variants=variants
    driver.HEADER_OVERLAYS=minimal.overlays
    for path in driver.SOURCE_PATHS:
        variants(path, subprocess.check_output(['git','show',driver.REV+':'+path], cwd=driver.ROOT, text=True))
    driver.main()
