#!/usr/bin/env python3
"""Complete-TU 2x2 EIA6 copy expansion and observed float predicate."""
import playbook_small_patterns as d

def variants(path, source):
    loop='\t\tfor (i = 0; i < 18; i++)\n\t\t\tV90PW(p)[0x88 / 4 + i] = V90PW(p)[0x110 / 4 + i];'
    expansion='\n'.join('\t\tV90PW(p)[0x%03x / 4] = V90PW(p)[0x%03x / 4];' % (0x88+4*i,0x110+4*i) for i in range(18))
    predicate='if (xf < 0.0f || xf > 0.0f)'
    assert source.count(loop)==1 and source.count(predicate)==1
    cells={}
    for expanded in (False,True):
        for unequal in (False,True):
            label=('expanded' if expanded else 'rolled')+'-'+('unequal' if unequal else 'relational')
            if not expanded and not unequal: label='baseline'
            text=source.replace(loop,expansion) if expanded else source
            if unequal:text=text.replace(predicate,'if (xf != 0.0f)')
            cells[label]=text
    assert len(set(cells.values()))==4
    return cells

if __name__=='__main__':
    d.REV='548b5edd';d.SOURCE_PATHS=('src/pump/v90/V90PreFilter.cpp',)
    d.OUT_NAME='batch-cpp-prefilter-expand';d.DUMP_FLAGS=('-v','-save-temps','-da','-dP')
    d.variants=variants;d.main()
