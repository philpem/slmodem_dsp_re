#!/usr/bin/env python3
"""Finite original pointer-index and terminal-return boundary controls."""
import playbook_small_patterns as d

def variants(path, source):
    cells={'baseline':source}
    if path.endswith('V90Demapper.cpp'):
        a,z,fn=d.function(source,'V90Demapper::process')
        swaps=[('out + total + signBitsPerFrame','out + (total + signBitsPerFrame)'),
               ('out + total +\n\t\t\t\t\t\t (signBitGroupSize - 1) * g','out + (total +\n\t\t\t\t\t\t (signBitGroupSize - 1) * g)'),
               ('out[total + b]','out[total + b]')]
        for old,new in swaps:
            assert fn.count(old)==1,(old,fn)
            fn=fn.replace(old,new)
        cells['integer-index-owner']=source[:a]+fn+source[z:]
    else:
        a,z,fn=d.function(source,'ResamplerTiming::addPhase')
        old='\tif (v > 0.0f) {\n'
        assert fn.count(old)==1
        fn=fn.replace(old,'\tif (!(v > 0.0f))\n\t\treturn;\n\n',1)
        assert fn.endswith('\t}\n}')
        fn=fn[:-4]+'}'
        lines=fn.splitlines();fn='\n'.join(line[1:] if line.startswith('\t\t') else line for line in lines)
        cells['guard-return']=source[:a]+fn+source[z:]
    return cells
if __name__=='__main__':
    d.REV='8af3af53';d.OUT_NAME='next20-v90-boundaries'
    d.SOURCE_PATHS=('src/pump/v90/V90Demapper.cpp','src/pump/v90/ResamplerTiming.cpp')
    d.DUMP_FLAGS=('-v','-save-temps','-da');d.variants=variants;d.main()
