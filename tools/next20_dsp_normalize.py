#!/usr/bin/env python3
"""Two complete-TU local-output normalization controls, without table/domain changes."""
import playbook_small_patterns as d

def variants(path, source):
    start,end,fn=d.function(source,'FPM_log10')
    old='\tnorm = mantissa;\n\tshifts = 0;\n\tif (norm <= 0x3fff) {\n\t\tdo {\n\t\t\tnorm = (unsigned short)(norm << 1);\n\t\t\tshifts++;\n\t\t} while (norm <= 0x3fff);\n\t}'
    assert fn.count(old)==1
    helper="""static inline void
normalize16(unsigned short value, unsigned short *mantissa, unsigned short *count)
{
	*count = 0;
	if (value <= 0x3fff) {
		int n = 0;
		do {
			value = (unsigned short)(value << 1);
			n++;
		} while (value <= 0x3fff);
		*count = (unsigned short)n;
	}
	*mantissa = value;
}

"""
    assert source[:start].endswith('short\n')
    candidate=source[:start-6]+helper+'short\n'+fn.replace(old,'\tnormalize16(mantissa, &norm, &shifts);')+source[end:]
    return {'baseline':source,'output-helper':candidate}

if __name__=='__main__':
    d.REV='8af3af53'
    d.SOURCE_PATHS=('src/dsp/fpm_log10.c',)
    d.OUT_NAME='next20-dsp-normalize'
    d.DUMP_FLAGS=('-v','-save-temps','-da')
    d.variants=variants
    d.main()
