#!/usr/bin/env python3
"""Object-witnessed sqrt_dp output helper, exponent-halving, word index cross."""
import itertools
import playbook_small_patterns as d

def variants(path,source):
    start,end,fn=d.function(source,'FPM_sqrt_dp')
    old="""	while (x <= 0x1fffffffu) {
		x += x;
		exponent++;
	}

	/* Truncated to 16 bits, which is where large inputs lose their top bit. */
	mantissa = (unsigned short)(x >> 15);"""
    assert fn.count(old)==1
    helper="""static inline void
normalize_sqrt32(unsigned int value, unsigned short *mantissa, unsigned short *count)
{
	*count = 0;
	if (value <= 0x1fffffffu) {
		int n = 0;
		do {
			value += value;
			n++;
		} while (value <= 0x1fffffffu);
		*count = (unsigned short)n;
	}
	*mantissa = (unsigned short)(value >> 15);
}

"""
    cells={}
    for output,half,word in itertools.product((False,True),repeat=3):
        body=fn
        prefix=source[:start]
        if output:
            body=body.replace('unsigned exponent = 0;', 'unsigned short exponent;').replace('unsigned mantissa;', 'unsigned short mantissa;')
            body=body.replace(old,'\tnormalize_sqrt32(x, &mantissa, &exponent);')
            assert prefix.endswith('unsigned short\n')
            prefix=prefix[:-15]+helper+'unsigned short\n'
        if half:
            body=body.replace('\tint index;', '\tint index;\n\tunsigned half_exponent;')
            body=body.replace('\tif (exponent & 1)', '\thalf_exponent = exponent >> 1;\n\tif (exponent != half_exponent * 2)')
            body=body.replace('(exponent >> 1)', 'half_exponent')
        if word:
            body=body.replace('\tint index;', '\tunsigned short index;')
        label='baseline' if not(output or half or word) else 'output%d-half%d-word%d'%(output,half,word)
        cells[label]=prefix+body+source[end:]
    assert len(set(cells.values()))==8
    return cells

if __name__=='__main__':
    d.REV='8af3af53'
    d.SOURCE_PATHS=('src/dsp/fpm_sqrt.c',)
    d.OUT_NAME='next20-dsp-sqrt-normalize'
    d.DUMP_FLAGS=('-v','-save-temps','-da')
    d.variants=variants
    d.main()
