#!/usr/bin/env python3
"""Replay timing-history index definitions around the original getter call."""
import sys
from pathlib import Path
import playbook_small_patterns as driver

def variants(path,source):
    cells={'baseline':source}
    original='\t\t\tunsigned int next = timingHistoryIndex + 1;\n\n'
    marker='\t\t\t    getTimingOffsetPPM();\n'
    assert source.count(original)==source.count(marker)==1
    late=source.replace(original,'').replace(marker,marker+'\t\t\tunsigned int next = timingHistoryIndex + 1;\n')
    cells['late-index-local']=late
    tail='\t\t\tunsigned int next = timingHistoryIndex + 1;\n\t\t\tif (next == timingHistoryLen)\n\t\t\t\tnext = 0;\n\t\t\ttimingHistoryIndex = next;'
    assert late.count(tail)==1
    cells['late-index-member']=late.replace(tail,'\t\t\ttimingHistoryIndex++;\n\t\t\tif (timingHistoryIndex == timingHistoryLen)\n\t\t\t\ttimingHistoryIndex = 0;')
    winner=(driver.ROOT/'build/gcc3-batch20-v90-resampler/V90Resampler/whole-complement-statement/V90Resampler.cpp').read_text()
    a,e,fn=driver.function(winner,'V90Resampler::getTimingHistoryStd')
    for label,text in list(cells.items()):
        start,end,old=driver.function(text,'V90Resampler::getTimingHistoryStd')
        cells[label+'-std-statements']=text[:start]+fn+text[end:]
    return cells

if __name__=='__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
    driver.REV='93d7eee1';driver.OUT_NAME='gcc3-batch20-v90-history'
    driver.SOURCE_PATHS=('src/pump/v90/V90Resampler.cpp',);driver.variants=variants;driver.main()
