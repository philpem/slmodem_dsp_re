#!/usr/bin/env python3
"""Preserve original SRE count normalization with a full RMS formal."""
import playbook_small_patterns as d
from gcc3_agc_rms_boundary import SOURCE_PATHS, variants as previous, headers


def variants(path,source):
    fixed=previous(path,source)['both']
    changed=fixed
    if path=='src/dsp/fpm_sre.c':
        old='FPM_rms(sre->rms_buf,\n\t\t\t\t\t\t       sre->cfg.rms_len)'
        assert fixed.count(old)==1
        changed=fixed.replace(old,old.replace('sre->cfg.rms_len','(unsigned short)sre->cfg.rms_len'))
    return {'baseline':fixed,'normalize':changed}


if __name__=='__main__':
    d.REV='6eaeaedd'
    d.SOURCE_PATHS=SOURCE_PATHS
    d.OUT_NAME='gcc3-agc-rms-normalize'
    d.DUMP_FLAGS=('-v','-save-temps','-da')
    d.variants=variants
    d.HEADER_OVERLAYS=lambda path,label:headers(path,'both')
    d.main()
