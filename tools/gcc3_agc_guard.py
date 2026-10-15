#!/usr/bin/env python3
"""Cross original eager AGC guard with use-only level narrowing."""
import playbook_small_patterns as d
from gcc3_agc_predicate import variants as previous


def variants(path,source):
    fixed=previous(path,source)['local-int']
    old='agc->freeze == 0 && level_est != 0'
    assert fixed.count(old)==1
    return {'baseline':fixed,
            'level-use':fixed.replace(old,'agc->freeze == 0 && (short)level_est != 0'),
            'eager':fixed.replace(old,'(agc->freeze == 0) & (level_est != 0)'),
            'both':fixed.replace(old,'(agc->freeze == 0) & ((short)level_est != 0)')}


if __name__=='__main__':
    d.REV='6eaeaedd'
    d.SOURCE_PATHS=('src/dsp/fpm_agc.c',)
    d.OUT_NAME='gcc3-agc-guard'
    d.DUMP_FLAGS=('-v','-save-temps','-da')
    d.variants=variants
    d.main()
