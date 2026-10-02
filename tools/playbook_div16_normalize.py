#!/usr/bin/env python3
"""Cross two independently observed 16-bit reciprocal source boundaries."""
import sys,subprocess
import playbook_small_patterns as driver

HELPER='''static inline void
normalize16(unsigned short denom, unsigned short *mantissa, unsigned short *count)
{
	int n = 0;
	if ((short)denom >= 0) {
		do {
			denom += denom;
			n++;
		} while ((short)denom >= 0);
		*count = (unsigned short)n;
	}
	*mantissa = denom;
}

'''

def variants(path,source):
    old='\t/* Left-normalise until the top bit is set, counting the shifts. */\n\twhile ((short)mantissa >= 0) {\n\t\tmantissa = (mantissa + mantissa) & 0xffff;\n\t\tcount++;\n\t}'
    assert source.count(old)==1
    cells={}
    for factoring in (False,True):
        for word in (False,True):
            text=source
            if factoring:
                marker='int\nFPM_div(unsigned short denom, unsigned short *recip, unsigned short *shift)'
                assert text.count(marker)==1
                text=text.replace(marker,HELPER+marker)
                text=text.replace('\tunsigned mantissa = denom;','\tunsigned short mantissa;').replace('\tunsigned count = 0;','\tunsigned short count = 0;')
                text=text.replace(old,'\tnormalize16(denom, &mantissa, &count);')
            if word:
                assert text.count('\tint index;')==1
                text=text.replace('\tint index;','\tunsigned short index;')
            label=('helper' if factoring else 'baseline')+('-word' if word else '')
            cells[label]=text
    assert len(cells)==len(set(cells.values()))==4
    return cells

if __name__=='__main__':
    assert '--domain' in sys.argv
    assert sys.argv[sys.argv.index('--domain')+1].startswith('https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-')
    driver.REV='a7dfe334'
    driver.OUT_NAME='playbook-div16-normalize'
    driver.SOURCE_PATHS=('src/dsp/fpm_div.c',)
    driver.variants=variants
    for path in driver.SOURCE_PATHS:
        variants(path,subprocess.check_output(['git','show',driver.REV+':'+path],cwd=driver.ROOT,text=True))
    driver.main()
