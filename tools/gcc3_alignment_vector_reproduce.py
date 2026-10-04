#!/usr/bin/env python3
"""Cross original vector-base and post-call level ownership in two callers."""
import itertools
import playbook_small_patterns as d

def variants(path,source):
    cells={}
    for direct,level in itertools.product((False,True),repeat=2):
        text=source
        for method,bits in [('generateJdPhase','jdV92PhaseBits'),('generateV92Jd','jdV92Bits')]:
            start,end,fn=d.function(text,'V90Phase3Modulator::'+method)
            old='\treturn scrambledSymbol(this, vectorBit('+bits+', symbolCount));'
            assert fn.count(old)==1
            arg=bits+'[(symbolCount - 1u) % 72u]' if direct else 'vectorBit('+bits+', symbolCount)'
            if level:
                new='\tint scrambled = scrambler.process('+arg+');\n\tshort level = codeLevel;\n\tpolarity ^= scrambled;\n\treturn polarity ? level : (short)-level;'
            else:new='\treturn scrambledSymbol(this, '+arg+');'
            text=text[:start]+fn.replace(old,new)+text[end:]
        label='baseline' if not(direct or level) else 'direct-%d-level-%d'%(direct,level)
        cells[label]=text
    return cells
if __name__=='__main__':
    d.REV='2001434c'
    d.SOURCE_PATHS=('src/pump/v90/V90Phase3Modulator.cpp',)
    d.OUT_NAME='gcc3-alignment-vector'
    d.DUMP_FLAGS=('-v','-save-temps','-da')
    d.variants=variants
    d.main()
