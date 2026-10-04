#!/usr/bin/env python3
"""Replay four frequency input-ownership and loop-boundary controls."""
import itertools
import sys
from pathlib import Path
import playbook_small_patterns as driver

def variants(path, source):
    start=source.index('void\nPsd::getFrequencies(')
    end=source.index('\n}\n',start)+3
    fn=source[start:end]
    loop='''\tfor (i = 0; i < bins; i++)
\t\tfreq[i] = (float)((long double)i * sampleRate *
\t\t\t\t  (1.0L / m_length));'''
    assert fn.count(loop)==1
    result={}
    for capture,guard in itertools.product((False,True),repeat=2):
        text=fn
        rate='rate' if capture else 'sampleRate'
        if guard:
            text=text.replace(loop,'''\ti = 0;
\tif (bins > 0) {
\t\tdo {
\t\t\tfreq[i] = (float)((long double)i * sampleRate *
\t\t\t\t\t  (1.0L / m_length));
\t\t} while (bins > ++i);
\t}''')
        if capture:
            text=text.replace('\tunsigned int i;', '\tlong double rate = sampleRate;\n\tunsigned int i;')
            text=text.replace('(long double)i * sampleRate', '(long double)i * rate')
        name='-'.join(x for x,v in [('capture-rate',capture),('guarded-do',guard)] if v) or 'baseline'
        result[name]=source[:start]+text+source[end:]
    assert len(result)==len(set(result.values()))==4
    return result

if __name__=='__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
    driver.REV='93d7eee1'
    driver.SOURCE_PATHS=('src/dsp/psd.cpp',)
    driver.OUT_NAME='gcc3-batch20-psd'
    driver.variants=variants
    driver.main()
