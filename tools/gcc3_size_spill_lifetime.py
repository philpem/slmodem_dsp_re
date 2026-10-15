#!/usr/bin/env python3
"""Test the named phase boundaries supported by unchanged reload observations."""
import sys
from pathlib import Path
import playbook_small_patterns as d


def variants(path, source):
    if path.endswith('V90MappingParamsInt.cpp'):
        a,z,fn=d.function(source,'getConstellationsIndex')
        old='\tunsigned int count = 1;\n\tunsigned int i;'
        assert fn.count(old)==1
        first=fn.replace(old,'\tunsigned int i;\n\tunsigned int count = 1;')
        scoped=fn.replace('\tunsigned int i;\n','').replace('for (i = 1; i <= 5; i++)','for (unsigned int i = 1; i <= 5; i++)')
        return {'baseline':source,'i-first':source[:a]+first+source[z:],
                'i-loop-scope':source[:a]+scoped+source[z:]}
    a,z,fn=d.function(source,'Psd::process')
    old='\tunsigned int frames = (count - m_overlap) / (m_length - m_overlap);\n\tunsigned int frame, i, j, pos;'
    assert fn.count(old)==1
    after=fn.replace(old,'\tunsigned int frames = (count - m_overlap) / (m_length - m_overlap);\n\tunsigned int frame, pos;\n\tunsigned int i, j;')
    before=fn.replace(old,'\tunsigned int frame, pos;\n\tunsigned int frames = (count - m_overlap) / (m_length - m_overlap);\n\tunsigned int i, j;')
    return {'baseline':source,'state-after-division':source[:a]+after+source[z:],
            'state-before-division':source[:a]+before+source[z:]}


if __name__=='__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
    d.REV='b6db3cfc'
    d.OUT_NAME='gcc3-size-spill-lifetime'
    d.SOURCE_PATHS=('src/pump/v90/V90MappingParamsInt.cpp','src/dsp/psd.cpp')
    d.DUMP_FLAGS=('-v','-save-temps','-dr','-dc','-dg')
    d.variants=variants
    d.main()
