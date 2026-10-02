#!/usr/bin/env python3
"""Restore observed AGC fourth arguments while preserving void return."""
import re
import playbook_small_patterns as driver

SOURCE_PATHS=('src/dsp/fpm_agc.c','src/pump/v32/V32int.c','src/pump/v32/V32rxhdx.c','src/pump/v23/v23rx.c','src/pump/v23/bwchdem.c','src/pump/v22/v22prc.c','src/pump/v22/V22int.c','src/pump/b103/B103prc.c','src/fax/V17r_int.c','src/fax/V21r_int.c','src/fax/V27r_int.c','src/fax/V29r_int.c')


def variants(path,source):
    text=source
    if path=='src/dsp/fpm_agc.c':
        old='FPM_AGC_agc(struct fpm_agc *agc, short *samples, unsigned short count)\n{'
        assert text.count(old)==1
        text=text.replace(old,old.replace('count)','count, int unused)')+'\n\t(void)unused;')
    else:
        positions=list(re.finditer(r'(?m)^[ \t]+FPM_AGC_agc\(',text))
        assert positions,path
        for match in reversed(positions):
            depth=1;end=match.end()
            while depth:
                if text[end]=='(':depth+=1
                elif text[end]==')':depth-=1
                end+=1
            call=text[match.start():end]
            value='1'
            if path.endswith('v23rx.c') and '&rx->agc,' in call:value='(short)count'
            if path.endswith('B103prc.c') and '&dsp->agc,' in call:value='original_count'
            text=text[:end-1]+', '+value+text[end-1:]
        if path.endswith('B103prc.c'):
            start,end,body=driver.function(text,'DemodDataB103')
            old='\tstruct b103_dsp *dsp = fp->dsp;'
            assert body.count(old)==1
            body=body.replace(old,old+'\n\tshort original_count = (short)count;')
            text=text[:start]+body+text[end:]
    assert text!=source
    return {'baseline':source,'fourth':text}


def overlays(path,label):
    if label=='baseline':return {}
    rel='dsplib/fpm_agc.h'
    text=(driver.ROOT/'include'/rel).read_text()
    old='void FPM_AGC_agc(struct fpm_agc *agc, short *samples, unsigned short count);'
    assert text.count(old)==1
    return {rel:text.replace(old,old.replace('count);','count, int unused);'))}


if __name__=='__main__':
    driver.REV='d3abfa0b'
    driver.OUT_NAME='playbook-agc-unused-argument'
    driver.SOURCE_PATHS=SOURCE_PATHS
    driver.variants=variants
    driver.HEADER_OVERLAYS=overlays
    driver.main()
