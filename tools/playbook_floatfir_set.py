#!/usr/bin/env python3
"""Four complete-TU coefficient-update/min-store controls for FloatFIR."""
import playbook_small_patterns as driver


def variants(path, source):
    start=source.index('FloatFIR::setCoefficients(')
    end=source.index('\n}\n',start)+2
    original=source[start:end]
    early='\tif (taps == want)\n\t\treturn 0;\n\n\ttaps = want;\n\troom = (int)(bufferLength - want);\n\tif (index > room)\n\t\tindex = room;'
    assert original.count(early)==1
    cells={}
    for label,minstore,nested in [('baseline',False,False),('min-store',True,False),('nested-update',False,True),('both',True,True)]:
        body=original
        if minstore:body=body.replace('\tif (index > room)\n\t\tindex = room;', '\tindex = index > room ? room : index;')
        if nested:
            lo=body.index('\tif (taps == want)');hi=body.index('\n\n\treturn 0;',lo)
            statements=body[lo:hi].split('\n\n',1)[1]
            body=body[:lo]+'\tif (taps != want) {\n\t'+statements.replace('\n','\n\t')+'\n\t}'+body[hi:]
        cells[label]=source[:start]+body+source[end:]
    assert len(cells)==len(set(cells.values()))==4
    return cells


if __name__=='__main__':
    driver.REV='599f0fa4'
    driver.OUT_NAME='playbook-floatfir-set'
    driver.SOURCE_PATHS=('src/dsp/FloatFIR.cpp',)
    driver.variants=variants
    driver.main()
