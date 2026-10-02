#!/usr/bin/env python3
"""Four complete-TU outer padding-guard controls for FloatARMA."""
import playbook_small_patterns as driver


def variants(path, source):
    cells = {}
    for label, denominator, numerator in [('baseline',False,False), ('den-guard',True,False), ('num-guard',False,True), ('both-guards',True,True)]:
        text=source
        for enabled,field,count,buffer in [(denominator,'m_nA','nDen','m_a'),(numerator,'m_nB','nNum','m_b')]:
            old='\tfor (m_idx = '+count+'; m_idx < '+field+'; m_idx++)\n\t\t'+buffer+'[m_idx] = 0.0f;'
            assert text.count(old)==1
            if enabled:text=text.replace(old,'\tif ('+field+' > '+count+') {\n\t'+old.replace('\n','\n\t')+'\n\t}')
        cells[label]=text
    assert len(cells)==len(set(cells.values()))==4
    return cells


if __name__ == '__main__':
    driver.REV='b550e9c6'
    driver.OUT_NAME='playbook-floatarma-padding'
    driver.SOURCE_PATHS=('src/dsp/FloatARMA.cpp',)
    driver.variants=variants
    driver.main()
