#!/usr/bin/env python3
"""Original short tap walk crossed with unsigned masked-symbol carrier."""
import playbook_small_patterns as d

def variants(path, source):
    a, z, fn = d.function(source, 'FSE_decision_trn')
    comment = ('\t\t\t/*\n\t\t\t * Hand over.  The empty loop the object runs over\n'
               '\t\t\t * `cfg.taps` here has no effect and is not\n'
               '\t\t\t * reproduced -- see finding F1604.\n\t\t\t */')
    assert fn.count(comment) == fn.count('\tint c;') == 1
    cells = {'baseline': source}
    for walk, unsigned in ((1, 0), (0, 1), (1, 1)):
        x = fn
        if walk:
            x = x.replace('\tshort n;', '\tshort n;\n\tshort k;')
            x = x.replace(comment, '\t\t\t/* Original short-counter tap walk (F1604). */')
            old = '\t\t\tm->rate_change = 1;'
            x = x.replace(old, old + '\n\t\t\tfor (k = 0; k < state->cfg.taps; k++)\n\t\t\t\t;')
        if unsigned:
            x = x.replace('\tint c;', '\tunsigned int c;')
        label = ('short-tap-walk-' if walk else '') + ('unsigned-symbol' if unsigned else 'signed-symbol')
        cells[label] = source[:a] + x + source[z:]
    return cells

if __name__ == '__main__':
    d.REV = '8af3af53'
    d.SOURCE_PATHS = ('src/pump/v32/V32dec.c',)
    d.OUT_NAME = 'next20-data-fse-trn'
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da')
    d.variants = variants
    d.main()
