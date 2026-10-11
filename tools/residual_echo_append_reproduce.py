#!/usr/bin/env python3
"""Cross original fast cursor/countdown with slow preincrement ownership."""
import playbook_small_patterns as d


def variants(path,source):
    a,z,fn=d.function(source,'V92EchoCanceller::updateEchoHistory')
    fast='\t\tfor (i = 0; i < count; i++)\n\t\t\t*w++ = arma->process(in[i]);'
    assert fn.count(fast)==1
    cells={'baseline':source}
    for cursor,increment in ((1,0),(0,1),(1,1)):
        text=fn
        if cursor:
            text=text.replace('\tunsigned int i;\n','')
            text=text.replace(fast,'\t\tunsigned int remaining = count;\n\n\t\tif (remaining != 0) {\n\t\t\tdo {\n\t\t\t\t*w++ = arma->process(*in++);\n\t\t\t} while (--remaining != 0);\n\t\t}')
        if increment:
            old='\t\tif (echoLength + 1 == historyAlloc) {'
            assert text.count(old)==1
            text=text.replace(old,'\t\tif (++echoLength == historyAlloc) {')
            text=text.replace('float *src = &echoHistory[echoLength];','float *src = &echoHistory[echoLength - 1];')
            text=text.replace('\t\t} else {\n\t\t\techoLength++;\n\t\t}','\t\t}')
        cells[f'cursor-{cursor}-increment-{increment}']=source[:a]+text+source[z:]
    assert len(set(cells.values()))==4
    return cells


if __name__=='__main__':
    d.REV='5b552950';d.OUT_NAME='residual-echo-append'
    d.SOURCE_PATHS=('src/pump/v90/V92EchoCanceller.cpp',)
    d.DUMP_FLAGS=('-v','-save-temps','-da','-dP');d.variants=variants;d.main()
