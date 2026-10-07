#!/usr/bin/env python3
"""Cross four original operand/use boundaries in TxHdxEQCondV27."""
import playbook_small_patterns as d

def variants(path,source):
    start,end,original=d.function(source,'TxHdxEQCondV27')
    cells={'baseline':source}
    for mask in range(1,16):
        fn=original
        if mask&1:
            assert fn.count('\tshort taken;')==1
            fn=fn.replace('\tshort taken;','\tunsigned short taken;')
            old='\ttaken = (short)((countdown <= *budget) ? countdown : *budget);'
            new='\ttaken = (unsigned short)((countdown <= (unsigned short)*budget)\n\t\t\t\t ? countdown : (unsigned short)*budget);'
            assert fn.count(old)==1;fn=fn.replace(old,new)
        if mask&2:
            old='\tprm->countdown = (short)(countdown - taken);'
            assert fn.count(old)==1;fn=fn.replace(old,'\tprm->countdown -= taken;')
        if mask&4:
            assert fn.count('\t\tshort rate;\n')==1
            assert fn.count('\t\trate = prm->rate;\n')==1
            fn=fn.replace('\t\tshort rate;\n','').replace('\t\trate = prm->rate;\n','')
            fn=fn.replace('V27TX_PATTERN_ALT[rate]','V27TX_PATTERN_ALT[prm->rate]')
            fn=fn.replace('V27TX_PATTERN_CARR[rate]','V27TX_PATTERN_CARR[prm->rate]')
        if mask&8:
            old='''\t\tfor (i = 0; i < taken; i++) {
\t\t\tif (in[i + 1] & 0x04)
\t\t\t\tin[i] = (unsigned short)V27TX_PATTERN_ALT['''
            assert fn.count(old)==1
            loop_start=fn.index('\t\tfor (i = 0; i < taken; i++) {',fn.index('ScrambleDataV27'))
            loop_end=fn.index('\n\t\t}',loop_start)+4
            loop=fn[loop_start:loop_end]
            loop=loop.replace('\t\tfor (i = 0; i < taken; i++) {','\t\tunsigned short left = (unsigned short)taken;\n\t\tunsigned short *cursor = in;\n\n\t\twhile (left--) {\n\t\t\tunsigned short *word = cursor++;')
            loop=loop.replace('in[i + 1]','word[1]').replace('in[i]','*word')
            fn=fn[:loop_start]+loop+fn[loop_end:]
        cells['c'+str(mask&1)+'-m'+str((mask>>1)&1)+'-r'+str((mask>>2)&1)+'-p'+str((mask>>3)&1)]=source[:start]+fn+source[end:]
    assert len(cells)==len(set(cells.values()))==16
    return cells
if __name__=='__main__':
    d.REV='9025b8d8';d.SOURCE_PATHS=('src/fax/V27t_prc.c',);d.OUT_NAME='fax-eqcond-reloads'
    d.DUMP_FLAGS=('-v','-save-temps','-da','-dP');d.variants=variants;d.main()
