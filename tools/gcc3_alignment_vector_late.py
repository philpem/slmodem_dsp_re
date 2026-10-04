#!/usr/bin/env python3
"""Test original single late return narrowing, preserving wide NEG ownership."""
import playbook_small_patterns as d

def variants(path,source):
    cells={'baseline':source}
    for unsigned in (False,True):
        text=source
        for method,bits in [('generateJdPhase','jdV92PhaseBits'),('generateV92Jd','jdV92Bits')]:
            start,end,fn=d.function(text,'V90Phase3Modulator::'+method)
            old='\treturn scrambledSymbol(this, vectorBit('+bits+', symbolCount));'
            assert fn.count(old)==1
            decl='unsigned int level = (unsigned short)codeLevel;' if unsigned else 'int level = codeLevel;'
            new='\tint scrambled = scrambler.process('+bits+'[(symbolCount - 1u) % 72u]);\n\t'+decl+'\n\tpolarity ^= scrambled;\n\tif (!polarity)\n\t\tlevel = -level;\n\treturn (short)level;'
            text=text[:start]+fn.replace(old,new)+text[end:]
        cells['late-unsigned-%d'%unsigned]=text
    return cells
if __name__=='__main__':
    d.REV='2001434c';d.SOURCE_PATHS=('src/pump/v90/V90Phase3Modulator.cpp',)
    d.OUT_NAME='gcc3-alignment-vector-late';d.DUMP_FLAGS=('-v','-save-temps','-da')
    d.variants=variants;d.main()
