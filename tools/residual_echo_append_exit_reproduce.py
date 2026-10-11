#!/usr/bin/env python3
"""Bound guard-owned countdown and shared return graph after cursor recovery."""
import playbook_small_patterns as d
import residual_echo_append_reproduce as earlier


def variants(path,source):
    combined=earlier.variants(path,source)['cursor-1-increment-1']
    a,z,fn=d.function(combined,'V92EchoCanceller::updateEchoHistory')
    cells={'baseline':source,'combined-repeat':combined}
    for shared,late in ((1,0),(0,1),(1,1)):
        text=fn
        if late:
            old='\t\tunsigned int remaining = count;\n\n\t\tif (remaining != 0) {'
            assert text.count(old)==1
            text=text.replace(old,'\t\tif (count != 0) {\n\t\t\tunsigned int remaining = count;')
        if shared:
            old='\t\treturn;\n\t}\n\n\twhile (count != 0) {'
            assert text.count(old)==1
            text=text.replace(old,'\t} else {\n\twhile (count != 0) {')
            tail=text.index('\twhile (count != 0) {')
            text=text[:tail]+''.join('\t'+line+'\n' if line else '\n' for line in text[tail:-1].splitlines())+'\t}\n}'
        cells[f'shared-{shared}-late-{late}']=combined[:a]+text+combined[z:]
    assert len(set(cells.values()))==5
    return cells


if __name__=='__main__':
    d.REV='5b552950';d.OUT_NAME='residual-echo-append-exit'
    d.SOURCE_PATHS=('src/pump/v90/V92EchoCanceller.cpp',)
    d.DUMP_FLAGS=('-v','-save-temps','-da','-dP');d.variants=variants;d.main()
