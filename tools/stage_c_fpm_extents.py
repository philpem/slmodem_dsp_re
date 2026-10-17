#!/usr/bin/env python3
"""Bound original FPM table extents independently of sqrt's invented clamp."""
import playbook_small_patterns as d


def recover_tables(source):
    source = source.replace('#define FPM_SQRT_TABLE_SIZE 193', '#define FPM_SQRT_TABLE_SIZE 192')
    source = source.replace('#define FPM_DIV_TABLE_ALL (128 + 1)', '#ifdef DSPLIB_REPRODUCE_BUGS\n#define FPM_DIV_TABLE_ALL 128\n#else\n#define FPM_DIV_TABLE_ALL 129\n#endif')
    source = source.replace('#ifdef DSPLIB_REPRODUCE_BUGS\n\t0\n#else\n\t16384\n#endif', '#ifndef DSPLIB_REPRODUCE_BUGS\n\t16384\n#endif')
    old = '\t32768,\t/* index 192: absent from the original\'s table - see above */\n'
    assert source.count(old) == 1
    return source.replace(old, '')


def recover_log(source):
    source = source.replace('FPM_log10_table[129]', 'FPM_log10_table[128]')
    start = source.index('\t/*\n\t * Entry 128 is NOT log10 of anything.')
    end = source.index('\n};', start)
    return source[:start] + source[end:]


def unclamp(source):
    start = source.index('\t/*\n\t * Within Q15 the index cannot leave')
    end = source.index('\n\treturn (unsigned short)(FPM_sqrt_table[index]', start)
    return source[:start] + source[end:]


def variants(path, source):
    if path.endswith('fpm_tables.c'): return {'baseline': source, 'original-extents': recover_tables(source)}
    if path.endswith('fpm_log10.c'): return {'baseline': source, 'original-extent': recover_log(source)}
    if path.endswith('fpm_sqrt.c'):
        no_clamp = unclamp(source)
        return {'baseline': source, 'no-invented-clamp': no_clamp,
                'original-source': no_clamp.replace('#define FPM_SQRT_TABLE_SIZE 193', '#define FPM_SQRT_TABLE_SIZE 192')}
    return {'baseline': source}


if __name__ == '__main__':
    d.REV = 'b856fde4'
    d.SOURCE_PATHS = ('src/dsp/fpm_tables.c', 'src/dsp/fpm_log10.c', 'src/dsp/fpm_sqrt.c', 'src/dsp/fpm_div.c', 'src/dsp/fpm_div32.c')
    d.OUT_NAME = 'stage-c-fpm-extents'
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dP')
    d.variants = variants
    d.main()
