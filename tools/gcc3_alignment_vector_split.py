#!/usr/bin/env python3
"""Cross separately evidenced late/early post-call ownership in sister methods."""
import itertools
import playbook_small_patterns as d

def variants(path,source):
    cells={}
    for phase,v92 in itertools.product((False,True),repeat=2):
        text=source
        for enabled,method,bits in [(phase,'generateJdPhase','jdV92PhaseBits'),(v92,'generateV92Jd','jdV92Bits')]:
            if not enabled:continue
            start,end,fn=d.function(text,'V90Phase3Modulator::'+method)
            old='\treturn scrambledSymbol(this, vectorBit('+bits+', symbolCount));'
            assert fn.count(old)==1
            arg=bits+'[(symbolCount - 1u) % 72u]'
            if method=='generateJdPhase':
                new='\tpolarity ^= scrambler.process('+arg+');\n\treturn polarity ? codeLevel : (short)-codeLevel;'
            else:
                new='\tint scrambled = scrambler.process('+arg+');\n\tunsigned int level = (unsigned short)codeLevel;\n\tpolarity ^= scrambled;\n\tif (!polarity)\n\t\tlevel = -level;\n\treturn (short)level;'
            text=text[:start]+fn.replace(old,new)+text[end:]
        label='baseline' if not(phase or v92) else 'phase-%d-v92-%d'%(phase,v92)
        cells[label]=text
    return cells
if __name__=='__main__':
    d.REV='2001434c';d.SOURCE_PATHS=('src/pump/v90/V90Phase3Modulator.cpp',)
    d.OUT_NAME='gcc3-alignment-vector-split';d.DUMP_FLAGS=('-v','-save-temps','-da')
    d.variants=variants;d.main()
