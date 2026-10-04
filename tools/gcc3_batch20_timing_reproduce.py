#!/usr/bin/env python3
"""Replay guarded phase-return and cold-step normalization source domains."""
import sys,itertools,re
from pathlib import Path
import playbook_small_patterns as driver

def variants(path,source):
    cells={'baseline':source}
    start,end,fn=driver.function(source,'ResamplerTiming::addPhase')
    for guard,local in itertools.product((False,True),repeat=2):
        if not guard and not local:continue
        body=fn
        if guard:
            head=body[:body.index('\tif (v > 0.0f) {')]
            inner=body[body.index('\tif (v > 0.0f) {')+len('\tif (v > 0.0f) {\n'):body.rindex('\n\t}')]
            inner='\n'.join(s[1:] if s.startswith('\t') else s for s in inner.split('\n'))
            body=head+'\tif (!(v > 0.0f))\n\t\treturn;\n'+inner+'\n}'
        if local:
            pos=body.index('phase +=')
            indent=body[body.rfind('\n',0,pos)+1:pos]
            body=body[:pos]+'double nextPhase = phase;\n'+indent+body[pos:]
            begin=body.index('phase +=');tail=re.sub(r'\bphase\b','nextPhase',body[begin:])
            finish=tail.rfind('\n\t}') if not guard else tail.rfind('\n}')
            tail=tail[:finish]+'\n'+indent+'phase = nextPhase;'+tail[finish:]
            body=body[:begin]+tail
        label='addphase-'+('-'.join(n for n,v in [('early-return',guard),('local-phase',local)] if v))
        cells[label]=source[:start]+body+source[end:]
    start,end,fn=driver.function(source,'ResamplerTiming::timingCorrection')
    marker='\tif (halfBaudStep & 1) {'
    hotstart=fn.index(marker)+len(marker+'\n')
    midd=fn.index('\n\t} else {',hotstart)
    coldstart=midd+len('\n\t} else {\n');finish=fn.rindex('\n\t}')
    for normalize,invert in itertools.product(('zero','modulo','mask'),(False,True)):
        if normalize=='zero' and not invert:continue
        body=fn
        if normalize!='zero':body=body.replace('halfBaudStep = 0;', 'halfBaudStep '+('%= 2;' if normalize=='modulo' else '&= 1;'))
        if invert:
            currentfinish=body.rindex('\n\t}')
            hot=body[hotstart:midd];cold=body[coldstart:currentfinish]
            body=body[:body.index(marker)]+'\tif (!(halfBaudStep & 1)) {\n'+cold+'\n\t} else {\n'+hot+body[currentfinish:]
        cells['timing-'+normalize+('-even-first' if invert else '-odd-first')]=source[:start]+body+source[end:]
    return cells

if __name__=='__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
    driver.REV='93d7eee1';driver.OUT_NAME='gcc3-batch20-v90-timing'
    driver.SOURCE_PATHS=('src/pump/v90/ResamplerTiming.cpp',);driver.variants=variants;driver.main()
