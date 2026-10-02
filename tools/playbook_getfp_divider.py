#!/usr/bin/env python3
"""Five complete-TU original-divider and abs-promotion controls."""
import playbook_small_patterns as driver


def variants(path, source):
    scaffold=source.replace('#include "dsplib/fp_math.h"', '#include <stdlib.h>\n\n#include "dsplib/fp_math.h"')
    assert scaffold!=source
    start=scaffold.index('GetFP_Value(short a, short b)');end=scaffold.index('\n}\n',start)+2
    original=scaffold[start:end]
    loop='''GetFP_Value(short a, short b)
{
	int va = a < 0 ? -a : a;
	int vb = b < 0 ? -b : b;
	int remaining = va << 14;
	short count = 0;

	while (remaining > 0) {
		remaining -= vb;
		count++;
	}

	return (short)(((int)a * (int)b) > 0 ? (short)count : (short)(-count));
}'''
    cells={'baseline':source,'abs-scaffold':scaffold}
    for label,body,builtin in [('divide-abs',original,True),('loop-ternary',loop,False),('loop-abs',loop,True)]:
        if builtin:
            body=body.replace('a < 0 ? -a : a', 'abs((int)a)').replace('b < 0 ? -b : b', 'abs((int)b)')
        cells[label]=scaffold[:start]+body+scaffold[end:]
    assert len(cells)==len(set(cells.values()))==5
    return cells


if __name__=='__main__':
    driver.REV='4188d010'
    driver.OUT_NAME='playbook-getfp-divider'
    driver.SOURCE_PATHS=('src/dsp/FP_math.c',)
    driver.variants=variants
    driver.main()
