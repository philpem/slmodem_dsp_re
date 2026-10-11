#!/usr/bin/env python3
"""Test the original +0xa00 typed owner against the retained flattened map."""
import itertools
import sys
from pathlib import Path
import playbook_small_patterns as d
from gcc3_v34_putframe_capture import variants as capture_variants

FIELDS = ('span', 'remainder', 'wide_accum', 'wide_bits', 'wide_bits_alt', 'group_count', 'idx_width')


def variants(path, source):
    previous = capture_variants(path, source)
    cells = {'baseline': source}
    for owner, early in itertools.product((False, True), repeat=2):
        text = previous['full-1-early-%d' % early]
        if owner:
            a, z, fn = d.function(text, 'putFrame')
            fn = fn.replace('\tv34_putbits_fn put = s->put_bits;',
                            '\tstruct v34_shell_frame_fields *cfg = &s->frame_fields;\n\tv34_putbits_fn put = s->put_bits;')
            for field in sorted(FIELDS, key=len, reverse=True):
                assert 's->' + field in fn
                fn = fn.replace('s->' + field, 'cfg->' + field)
            text = text[:a] + fn + text[z:]
        cells['owner-%d-early-%d' % (owner, early)] = text
    return cells


def overlays(path, label):
    if label == 'baseline' or label.startswith('owner-0-'):
        return {}
    text = (d.ROOT / 'include/dsplib/v34shell.h').read_text()
    start = text.index('\tshort           span;', text.index('struct v34_shell {'))
    end = text.index('\n\t/*\n\t * decodeDepth', start)
    fields = text[start:end]
    definition = 'struct v34_shell_frame_fields {\n' + fields + '\n};\n\n'
    text = text[:start] + '\tunion {\n\t\tstruct v34_shell_frame_fields frame_fields;\n\t\tstruct {\n' + fields + '\n\t\t};\n\t};' + text[end:]
    text = text.replace('struct v34_shell {', definition + 'struct v34_shell {', 1)
    return {'dsplib/v34shell.h': text}


if __name__ == '__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain') + 1]).is_file()
    d.REV = '7edfb734'
    d.OUT_NAME = 'gcc3-v34-putframe-owner'
    d.SOURCE_PATHS = ('src/pump/v34/v34shell.c',)
    d.variants = variants
    d.HEADER_OVERLAYS = overlays
    d.main()
