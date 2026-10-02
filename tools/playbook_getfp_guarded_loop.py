#!/usr/bin/env python3
"""Four full-TU positive-remainder guard and scoped-divisor loop controls."""
import playbook_small_patterns as driver
import playbook_getfp_negative_step as negative


def variants(path, source):
    seed=negative.variants(path,source)['outside-negative']
    cells={'baseline':source,'staged-outside':seed}
    old='''	int step = -abs((int)b);

	while (remaining > 0) {
		remaining += step;
		count++;
	}'''
    assert seed.count(old)==1
    for label,do in [('guarded-do',True),('guarded-while',False)]:
        block='\tif (remaining > 0) {\n\t\tint step = -abs((int)b);\n\n'
        block+='\t\tdo {\n' if do else '\t\twhile (remaining > 0) {\n'
        block+='\t\t\tremaining += step;\n\t\t\tcount++;\n'
        block+='\t\t} while (remaining > 0);\n' if do else '\t\t}\n'
        block+='\t}'
        cells[label]=seed.replace(old,block)
    assert len(cells)==len(set(cells.values()))==4
    return cells


if __name__=='__main__':
    driver.REV='4188d010'
    driver.OUT_NAME='playbook-getfp-guarded-loop'
    driver.SOURCE_PATHS=('src/dsp/FP_math.c',)
    driver.variants=variants
    driver.main()
