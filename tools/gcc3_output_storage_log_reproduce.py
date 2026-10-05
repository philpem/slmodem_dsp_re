#!/usr/bin/env python3
"""Separate a logarithm table-result owner from a compound return expression."""
import playbook_small_patterns as d
import gcc3_normalization_owner_reproduce as prior

def variants(path,source):
    control=prior.variants(path,source)['loop-carried-word-output']
    old="\treturn (short)((FPM_log10_table[idx] >> 3) - (int)shifts * 1228);"
    assert control.count(old)==1
    cells={'baseline':source,'pointed-control':control}
    for width in ['short','int']:
        new="\t"+width+" result = FPM_log10_table[idx] >> 3;\n\tresult -= (int)shifts * 1228;\n\treturn result;"
        cells[width+'-table-result']=control.replace(old,new)
    assert len(cells)==len(set(cells.values()))==4
    return cells

if __name__=='__main__':
    d.REV='f7b95e35';d.OUT_NAME='gcc3-output-storage-log';d.SOURCE_PATHS=('src/dsp/fpm_log10.c',)
    d.DUMP_FLAGS=('-v','-save-temps','-da','-dP');d.variants=variants;d.main()
