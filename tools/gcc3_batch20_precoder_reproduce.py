#!/usr/bin/env python3
"""Replay finite original transfer-group expansions, no header or template changes."""
import sys,itertools
from pathlib import Path
import playbook_small_patterns as driver

def variants(path,source):
    old='\tfor (i = 0; i < 6; i++) {\n\t\thead[i] = (unsigned int *)p->constellations[i];\n\t\ttableB[i] = (int)p->LC[i];\n\t}'
    coef='\tfor (i = 0; i < 12; i++)\n\t\ttableA[i] = p->m[i];'
    cells={}
    for group,expandcoef in itertools.product(('loop','head-lc-pairs','lc-head-pairs','lc-head-groups','head-lc-groups'),(False,True)):
        text=source
        if group!='loop':
            heads=['\thead[%d] = (unsigned int *)p->constellations[%d];'%(i,i) for i in range(6)]
            lc=['\ttableB[%d] = (int)p->LC[%d];'%(i,i) for i in range(6)]
            lists={'head-lc-pairs':[v for pair in zip(heads,lc) for v in pair],'lc-head-pairs':[v for pair in zip(lc,heads) for v in pair],'lc-head-groups':lc+heads,'head-lc-groups':heads+lc}
            assert text.count(old)==1;text=text.replace(old,'\n'.join(lists[group]))
        if expandcoef:
            assert text.count(coef)==1;text=text.replace(coef,'\n'.join('\ttableA[%d] = p->m[%d];'%(i,i) for i in range(12)))
        cells[group+('-scalar-coefficients' if expandcoef else '') if group!='loop' or expandcoef else 'baseline']=text
    return cells

if __name__=='__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
    driver.REV='93d7eee1';driver.OUT_NAME='gcc3-batch20-v90-precoder'
    driver.SOURCE_PATHS=('src/pump/v90/V92Precoder.cpp',);driver.variants=variants;driver.main()
