#!/usr/bin/env python3
"""Compare explicit AGC mask with raw/byte-count source hypotheses."""
import playbook_small_patterns as d


def variants(path,source):
    old='v = (int)((unsigned)v << (shift & 31));'
    assert source.count(old)==1
    cells={'baseline':source}
    for label,count in [('raw','shift'),('byte','(unsigned char)shift'),('byte-mask','((unsigned char)shift & 31)')]:
        cells[label]=source.replace(old,'v = (int)((unsigned)v << '+count+');')
    assert len(set(cells.values()))==4
    return cells


if __name__=='__main__':
    d.REV='1ffa3fb9'
    d.SOURCE_PATHS=('src/dsp/fpm_agc.c',)
    d.OUT_NAME='gcc3-agc-shift'
    d.DUMP_FLAGS=('-v','-save-temps','-da')
    d.variants=variants
    d.main()
