#!/usr/bin/env python3
"""Four V27 controls for independently witnessed post-FIFO budget capture."""
from itertools import product
import playbook_small_patterns as driver
import recovery_v27_data_boundaries as first
from recovery_sre_boundary import REV

def variants(path,source):
    cells={'baseline':source}
    for formal,capture in product((0,1),repeat=2):
        text=first.variants(path,source)['f%d_w1_o1_s1'%formal]
        start,end,body=driver.function(text,'TxHdxDataV27')
        if capture:
            old='in, (unsigned short)taken);\n\n\tif ((unsigned short)*budget <= got)'
            new='in, (unsigned short)taken);\n\ttaken = (unsigned short)*budget;\n\n\tif (taken <= got)'
            assert body.count(old)==1
            body=body.replace(old,new)
            old='short remaining = (short)((unsigned short)*budget - got);'
            assert body.count(old)==1
            body=body.replace(old,'short remaining = (short)(taken - got);')
            old='\n\ttaken = *budget;\n\tScrambleDataV27(modem, in, taken);'
            assert body.count(old)==1
            body=body.replace(old,'\n\tScrambleDataV27(modem, in, taken);')
        cells['f%d_capture%d'%(formal,capture)]=text[:start]+body+text[end:]
    assert len(cells) == 5 and len(set(cells.values())) == 3
    # The formal axis is the header overlay; paired source tokens coincide.
    return cells

if __name__ == '__main__':
    driver.REV=REV
    driver.OUT_NAME='recovery-v27-published-budget'
    driver.SOURCE_PATHS=('src/fax/V27t_prc.c',)
    driver.variants=variants
    driver.HEADER_OVERLAYS=first.overlays
    driver.DUMP_FLAGS=()
    driver.main()
