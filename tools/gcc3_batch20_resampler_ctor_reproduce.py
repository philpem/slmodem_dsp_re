#!/usr/bin/env python3
"""Cross Resampler constructor history-result ownership and size guards."""
import sys,itertools
from pathlib import Path
import playbook_small_patterns as driver

def variants(path,source):
    cells={}
    for borrowed,owned,sizeguard in itertools.product((False,True),repeat=3):
        text=source
        for bank,local in ((True,borrowed),(False,owned)):
            signature='float *bank, unsigned int minHistory)' if bank else 'float cutoff, unsigned int minHistory)'
            pos=text.index(signature);start=text.index('{',pos);end=text.index('\n}',start)+2
            body=text[start:end]
            if local:
                old='\thistory = 0;\n\tif (historyLen)\n\t\thistory = (float *)sysdep_malloc(historyLen * sizeof(float));'
                new='\tfloat *newHistory = 0;\n\tif (historyLen)\n\t\tnewHistory = (float *)sysdep_malloc(historyLen * sizeof(float));\n\thistory = newHistory;'
                assert body.count(old)==1;body=body.replace(old,new)
            if sizeguard:
                old='\thistoryLen = (taps < minHistory) ? minHistory : 10 * taps;'
                new='\thistoryLen = minHistory;\n\tif (taps >= minHistory)\n\t\thistoryLen = 10 * taps;'
                assert body.count(old)==1;body=body.replace(old,new)
            text=text[:start]+body+text[end:]
        cells['-'.join(n for n,v in [('borrowed-local',borrowed),('owned-local',owned),('size-guard',sizeguard)] if v) or 'baseline']=text
    return cells

if __name__=='__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
    driver.REV='93d7eee1';driver.OUT_NAME='gcc3-batch20-v90-resampler-ctor'
    driver.SOURCE_PATHS=('src/pump/v90/Resampler.cpp',);driver.variants=variants;driver.main()
