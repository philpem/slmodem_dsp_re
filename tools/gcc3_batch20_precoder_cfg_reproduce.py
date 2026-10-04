#!/usr/bin/env python3
"""Cross original normal/coset source arm organization in the precoder."""
import sys,itertools,re
from pathlib import Path
import playbook_small_patterns as driver

def swap(text,occurrence):
    positions=[m.start() for m in re.finditer(r'^\t+if \((?:i > 2|!\(i > 2\))\) \{',text,re.M)]
    assert len(positions)==2
    pos=sorted(positions)[occurrence];opening=text.index('{',pos)
    def close(start):
        depth=1
        for i in range(start+1,len(text)):
            if text[i]=='{':depth+=1
            elif text[i]=='}':
                depth-=1
                if not depth:return i
        raise AssertionError('unbalanced arms')
    first=close(opening);assert text[first:first+8]=='} else {'
    secondopening=first+7;second=close(secondopening)
    cond=text[pos:opening].replace('i > 2','!(i > 2)')
    return text[:pos]+cond+'{'+text[secondopening+1:second]+'} else {'+text[opening+1:first]+'}'+text[second+1:]

def variants(path,source):
    start,end,fn=driver.function(source,'V92Precoder::process')
    cells={}
    for bounds,index in itertools.product((False,True),repeat=2):
        body=fn
        if index:body=swap(body,1)
        if bounds:body=swap(body,0)
        cells['-'.join(n for n,v in [('normal-bounds-first',bounds),('normal-index-first',index)] if v) or 'baseline']=source[:start]+body+source[end:]
    return cells

if __name__=='__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
    driver.REV='93d7eee1';driver.OUT_NAME='gcc3-batch20-v90-precoder-cfg'
    driver.SOURCE_PATHS=('src/pump/v90/V92Precoder.cpp',);driver.variants=variants;driver.main()
