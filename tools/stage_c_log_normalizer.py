#!/usr/bin/env python3
"""Cross witnessed pointed-HI normalization with same-home exponent writeback."""
import playbook_small_patterns as d

HELPER = '''static inline void
log_normalize16(unsigned short value, unsigned short *mantissa, unsigned short *count)
{
	*count = 0;
	while (value <= 0x3fff) {
		value += value;
		(*count)++;
	}
	*mantissa = value;
}

'''


def variants(path, source):
    start, end, body = d.function(source, 'FPM_log10')
    loop_start = body.index('\tnorm = mantissa;')
    loop_end = body.index('\n\n\t/*', loop_start)
    old = body[loop_start:loop_end]
    cells = {}
    for pointed in (False, True):
        for writeback in (False, True):
            fn = body
            prefix = source[:start]
            if pointed:
                fn = fn.replace(old, '\tlog_normalize16(mantissa, &norm, &shifts);')
                assert prefix.endswith('short\n')
                prefix = prefix[:-6] + HELPER + 'short\n'
            if writeback:
                fn = fn.replace('\tunsigned short e;\n', '')
                fn = fn.replace('e = (unsigned short)(exponent + (short)shifts);', 'shifts = (unsigned short)(exponent + (short)shifts);')
                fn = fn.replace('(int)e * 1228', '(int)shifts * 1228')
            label = 'baseline' if not pointed and not writeback else 'pointed%d-writeback%d' % (pointed, writeback)
            cells[label] = prefix + fn + source[end:]
    assert len(set(cells.values())) == 4
    return cells


if __name__ == '__main__':
    d.REV = 'f8c8225c'
    d.SOURCE_PATHS = ('src/dsp/fpm_log10.c',)
    d.OUT_NAME = 'stage-c-log-normalizer'
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dP')
    d.variants = variants
    d.main()
