#!/usr/bin/env python3
"""Finite energy-validator result graph x callback-value-age controls."""
import playbook_small_patterns as d

def variants(path,source):
    start,end,body=d.function(source,'bValidateEnergyValue')
    cells={}
    original='hist[*idxp] = (int)(fComputeRMSValueFloatBuf(n, buf) * 32000.0f);'
    assert body.count(original)==1
    for result in (False,True):
        for sequenced in (False,True):
            fn=body
            if sequenced:
                fn=fn.replace('\tint avg;','\tint avg;\n\tfloat rms;')
                fn=fn.replace(original,'rms = fComputeRMSValueFloatBuf(n, buf);\n\thist[*idxp] = (int)(rms * 32000.0f);')
            if result:
                fn=fn.replace('\tint avg;','\tint avg;\n\tint accepted = 1;')
                fn=fn.replace('\tif (bInternalBeepInProgress)\n\t\treturn 0;', '\tif (bInternalBeepInProgress) {\n\t\taccepted = 0;\n\t\tgoto done;\n\t}')
                fn=fn.replace('\t\t\treturn 1;','\t\t\tgoto done;')
                assert fn.endswith('\treturn 0;\n}')
                fn=fn[:-len('\treturn 0;\n}')]+ '\taccepted = 0;\ndone:\n\treturn accepted;\n}'
            label=('common-success' if result else 'retained-result')+'-'+('sequenced-rms' if sequenced else 'retained-rms')
            if not result and not sequenced:label='baseline'
            cells[label]=source[:start]+fn+source[end:]
    assert len(set(cells.values()))==4
    return cells
if __name__=='__main__':
    d.REV='75e7ef4b';d.SOURCE_PATHS=('src/service/Fdspkrnl.c',);d.OUT_NAME='energy-callback-result';d.DUMP_FLAGS=('-v','-save-temps','-da','-dP');d.variants=variants;d.main()
