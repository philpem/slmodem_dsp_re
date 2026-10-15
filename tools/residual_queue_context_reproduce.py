#!/usr/bin/env python3
"""Replay the historically exact Queue full-query in a changed complete TU."""
import playbook_small_patterns as d

HEADER=(d.ROOT/'include/dsplib/Queue.h').read_text()
OLD='\tif (size - count() - 1 == 0)\n\t\treturn -1;'
START=HEADER.index('int Queue<T>::write(T v)')
assert HEADER[START:].count(OLD)==1


def variants(path,source):
    return {label:source for label in ('baseline','isfull','space-local')}


def overlays(path,label):
    if label=='baseline':return {}
    new=('\tif (isFull())\n\t\treturn -1;' if label=='isfull' else
         '\tunsigned int space = size - count() - 1;\n\n\tif (space == 0)\n\t\treturn -1;')
    text=HEADER[:START]+HEADER[START:].replace(OLD,new)
    return {'dsplib/Queue.h':text}


if __name__=='__main__':
    d.REV='57dbbf52';d.OUT_NAME='residual-queue-context'
    d.SOURCE_PATHS=('src/pump/v90/V92Modulator.cpp',)
    d.DUMP_FLAGS=('-v','-save-temps','-da','-dP');d.variants=variants
    d.HEADER_OVERLAYS=overlays;d.main()
