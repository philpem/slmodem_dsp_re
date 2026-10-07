#!/usr/bin/env python3
"""Bounded pre-Scramble lifetimes observed in original EQ conditioning."""
import playbook_small_patterns as d
import fax_eqcond_reload_reproduce as first

def variants(path,source):
    original=first.variants(path,source)['c1-m1-r1-p1']
    cells={'baseline':source,'all-reloads':original}
    for left,cursor in [(1,0),(0,1),(1,1)]:
        start,end,fn=d.function(original,'TxHdxEQCondV27')
        if left:
            old='\t\tunsigned short left = (unsigned short)taken;\n'
            assert fn.count(old)==1
            fn=fn.replace(old,'')
            fn=fn.replace('\tshort i;','\tshort i;\n\tunsigned short left;')
            fn=fn.replace('\tScrambleDataV27(modem, in, taken);','\tleft = taken;\n\tScrambleDataV27(modem, in, taken);')
        if cursor:
            old='\t\tunsigned short *cursor = in;\n'
            assert fn.count(old)==1
            fn=fn.replace(old,'')
            fn=fn.replace('\tshort i;','\tshort i;\n\tunsigned short *cursor;')
            fn=fn.replace('\tfor (i = 0; i < taken; i++)','\tcursor = in;\n\tfor (i = 0; i < taken; i++)',1)
        cells[f'preleft-{left}-precursor-{cursor}']=original[:start]+fn+original[end:]
    return cells
if __name__=='__main__':
    d.REV='9025b8d8';d.SOURCE_PATHS=('src/fax/V27t_prc.c',);d.OUT_NAME='fax-eqcond-lifetimes'
    d.DUMP_FLAGS=('-v','-save-temps','-da','-dP');d.variants=variants;d.main()
