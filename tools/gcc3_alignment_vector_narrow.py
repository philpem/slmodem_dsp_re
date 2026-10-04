#!/usr/bin/env python3
"""Trace the original narrow updated sample after independently recovered index."""
import itertools
import playbook_small_patterns as d

def variants(path,source):
    cells={'baseline':source}
    for direct,unsigned in itertools.product((False,True),repeat=2):
        text=source
        for method,bits in [('generateJdPhase','jdV92PhaseBits'),('generateV92Jd','jdV92Bits')]:
            start,end,fn=d.function(text,'V90Phase3Modulator::'+method)
            old='\treturn scrambledSymbol(this, vectorBit('+bits+', symbolCount));'
            assert fn.count(old)==1
            arg=bits+'[(symbolCount - 1u) % 72u]' if direct else 'vectorBit('+bits+', symbolCount)'
            decl='unsigned short level = (unsigned short)codeLevel;' if unsigned else 'short level = codeLevel;'
            result='(short)level' if unsigned else 'level'
            new='\tint scrambled = scrambler.process('+arg+');\n\t'+decl+'\n\tpolarity ^= scrambled;\n\tif (!polarity)\n\t\tlevel = -level;\n\treturn '+result+';'
            text=text[:start]+fn.replace(old,new)+text[end:]
        cells['direct-%d-unsigned-%d'%(direct,unsigned)]=text
    return cells
if __name__=='__main__':
    d.REV='2001434c';d.SOURCE_PATHS=('src/pump/v90/V90Phase3Modulator.cpp',)
    d.OUT_NAME='gcc3-alignment-vector-narrow';d.DUMP_FLAGS=('-v','-save-temps','-da')
    d.variants=variants;d.main()
