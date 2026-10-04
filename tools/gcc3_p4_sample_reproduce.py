#!/usr/bin/env python3
"""Cross unsigned wide magnitude/terminal narrowing in the two P4 generators.

F8141 bounded signed/unsigned short carriers, and the later amplitude cast
domain kept a short carrier. Original MOVZWL, full NEG and terminal MOVSWL
instead predict a wide magnitude with one signed narrowing. No field/ABI
changes, declaration permutations or pass controls.
"""
import itertools
import playbook_small_patterns as d

def variants(path, source):
    cells = {}
    for cpt, e1u in itertools.product((False, True), repeat=2):
        text = source
        for enabled, method in [(cpt, 'generateCPt'), (e1u, 'generateE1u')]:
            if not enabled:
                continue
            start, end, fn = d.function(text, 'int V92Phase4Modulator::'+method)
            for old in ['\tshort sym;', '\tsym = amplitude;', '\treturn sym;']:
                assert fn.count(old) == 1
            fn = fn.replace('\tshort sym;', '\tunsigned int sym;')
            fn = fn.replace('\tsym = amplitude;', '\tsym = (unsigned short)amplitude;')
            fn = fn.replace('\treturn sym;', '\treturn (short)sym;')
            text = text[:start]+fn+text[end:]
        label = 'baseline' if not (cpt or e1u) else 'cpt-%d-e1u-%d'%(cpt, e1u)
        cells[label] = text
    assert len(set(cells.values())) == 4
    return cells

if __name__ == '__main__':
    d.REV = '14769433'
    d.SOURCE_PATHS = ('src/pump/v90/V92Phase4Modulator.cpp',)
    d.OUT_NAME = 'gcc3-p4-sample'
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da')
    d.variants = variants
    d.main()
