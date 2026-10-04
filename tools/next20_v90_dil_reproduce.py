#!/usr/bin/env python3
"""Original code-argument age through the existing DIL segment helper."""
import itertools,json,sys
from pathlib import Path
import playbook_small_patterns as d
from gcc3_value_carriers_audit import inspect

def variants(path,source):
    cells={}
    for table,descriptor in itertools.product((False,True),repeat=2):
        text=source
        for enabled,signature,marker,finish,argument in [
            (table,'DilType type','int segBounds[2][8]','\n\t\tlength +=','TO[type][i]'),
            (descriptor,'tagV90DILdescriptor *dil','int codeSegmentsBoundries[2][8]','\n\t\t/*\n\t\t * THE OBJECT READS','dil->dilCode[i]')]:
            if not enabled:continue
            start=text.index('\ncalculateDilLength('+signature)+1
            end=text.index('\n}\n',start)+2
            fn=text[start:end];a=fn.index('\t\t'+marker);z=fn.index(finish,a)
            fn=fn[:a]+'\t\tunsigned int seg = getSegmentPointer(pcmType, '+argument+');\n'+fn[z:]
            text=text[:start]+fn+text[end:]
        cells['baseline' if not(table or descriptor) else 'helper-table-%d-descriptor-%d'%(table,descriptor)]=text
    return cells

if __name__=='__main__':
    d.REV='8af3af53';d.OUT_NAME='next20-v90-dil';d.SOURCE_PATHS=('src/pump/v90/V90DilDescriptorSettings.cpp',)
    d.DUMP_FLAGS=('-v','-save-temps','-da');d.variants=variants;d.main()
