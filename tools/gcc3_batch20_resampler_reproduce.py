#!/usr/bin/env python3
"""Replay declared resampler source boundaries on complete period translation units."""
import sys,itertools
from pathlib import Path
import playbook_small_patterns as driver

def variants(path,source):
    start,end,fn=driver.function(source,'V90Resampler::getTimingHistoryStd')
    cells={'baseline':source}
    ret='\treturn sqrt(var * (var < 0.0f ? -1.0f : 1.0f));'
    for pred in ('negative','nonnegative','whole-complement'):
        cond={'negative':'var < 0.0f','nonnegative':'var >= 0.0f','whole-complement':'!(var < 0.0f)'}[pred]
        first,second=('-1.0f','1.0f') if pred=='negative' else ('1.0f','-1.0f')
        for stmt in (False,True):
            if pred=='negative' and not stmt:continue
            if stmt:
                replacement='\tfloat sign;\n\n\tif ('+cond+')\n\t\tsign = '+first+';\n\telse\n\t\tsign = '+second+';\n\treturn sqrt(var * sign);'
            else:replacement='\treturn sqrt(var * ('+cond+' ? '+first+' : '+second+'));'
            assert fn.count(ret)==1
            cells[pred+('-statement' if stmt else '-ternary')]=source[:start]+fn.replace(ret,replacement)+source[end:]
    return cells

if __name__=='__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
    driver.REV='93d7eee1';driver.OUT_NAME='gcc3-batch20-v90-resampler'
    driver.SOURCE_PATHS=('src/pump/v90/V90Resampler.cpp',);driver.variants=variants;driver.main()
