#!/usr/bin/env python3
"""Cross borrowed Resampler member initialization with common history ownership."""
import sys,itertools
from pathlib import Path
import playbook_small_patterns as driver

def variants(path,source):
    cells={}
    signature='float *bank, unsigned int minHistory)'
    for init,local in itertools.product((False,True),repeat=2):
        text=source
        if init:
            assert text.count(signature)==1
            text=text.replace(signature,signature+'\n\t: coeffs(bank), ppmScale(scale), phases(nPhases), taps(nTaps),\n\t  coeffsBorrowed(1)')
            pos=text.index(signature);start=text.index('{',pos);end=text.index('\n}',start)+2
            body=text[start:end]
            for line in ('coeffs = bank;','phases = nPhases;','ppmScale = scale;','taps = nTaps;','coeffsBorrowed = 1;'):
                assert body.count('\t'+line)==1;body=body.replace('\t'+line+'\n','')
            text=text[:start]+body+text[end:]
        if local:
            pos=text.index(signature);start=text.index('{',pos);end=text.index('\n}',start)+2
            body=text[start:end]
            old='\thistory = 0;\n\tif (historyLen)\n\t\thistory = (float *)sysdep_malloc(historyLen * sizeof(float));'
            new='\tfloat *newHistory = 0;\n\tif (historyLen)\n\t\tnewHistory = (float *)sysdep_malloc(historyLen * sizeof(float));\n\thistory = newHistory;'
            assert body.count(old)==1;body=body.replace(old,new);text=text[:start]+body+text[end:]
        cells['-'.join(n for n,v in [('member-init',init),('local-history',local)] if v) or 'baseline']=text
    return cells

if __name__=='__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
    driver.REV='93d7eee1';driver.OUT_NAME='gcc3-batch20-v90-resampler-init'
    driver.SOURCE_PATHS=('src/pump/v90/Resampler.cpp',);driver.variants=variants;driver.main()
