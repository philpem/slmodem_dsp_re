#!/usr/bin/env python3
"""Cross original observed derived-field order with shared HI value use."""
import playbook_small_patterns as d


def variants(path, source):
    name='FPM_SDM_init' if path.endswith('fpm_sdm.c') else 'SDM_init'
    a,z,fn=d.function(source,name)
    old='\tsdm->shift2 = (short)(sdm->cfg.tap2 - sdm->cfg.nbits);\n'
    assert fn.count(old)==1
    marker='\tsdm->cfg = *cfg;\n'
    assert fn.count(marker)==1
    cells={}
    for shared,early in ((0,0),(1,0),(0,1),(1,1)):
        text=fn
        if early:
            text=text.replace(old,'').replace(marker,marker+old)
        if shared:
            text=text.replace('sdm->cfg.nbits','nbits')
            text=text.replace(marker,marker+'\tshort nbits = sdm->cfg.nbits;\n')
        cells['baseline' if not(shared or early) else f'shared-{shared}-early-{early}']=source[:a]+text+source[z:]
    assert len(set(cells.values()))==4
    return cells


if __name__=='__main__':
    d.REV='9e20a346'
    d.OUT_NAME='residual-sdm-init'
    d.SOURCE_PATHS=('src/dsp/fpm_sdm.c','src/fax/SDM.c')
    d.DUMP_FLAGS=('-v','-save-temps','-da','-dP')
    d.variants=variants
    d.main()
