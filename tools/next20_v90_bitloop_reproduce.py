#!/usr/bin/env python3
"""Shared shift latch in the original binary formatter."""
import playbook_small_patterns as d

def variants(path,source):
    a,z,fn=d.function(source,'V90MP::PrintBase2')
    assert fn.count('\twhile (mask != 0) {')==1 and fn.count('\t\tmask >>= 1;\n')==1
    fn=fn.replace('\twhile (mask != 0) {','\tfor (; mask != 0; mask >>= 1) {').replace('\t\tmask >>= 1;\n','')
    return {'baseline':source,'shared-shift-latch':source[:a]+fn+source[z:]}
if __name__=='__main__':
    d.REV='8af3af53';d.OUT_NAME='next20-v90-bitloop';d.SOURCE_PATHS=('src/pump/v90/V90MP.cpp',)
    d.DUMP_FLAGS=('-v','-save-temps','-da');d.variants=variants;d.main()
