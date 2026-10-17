#!/usr/bin/env python3
"""Restore the public writable sign tables and their witnessed neighbours."""
import re
import playbook_small_patterns as d


def variants(path, source):
    changed = source.replace('#include "dsplib/fpm_phasor.h"', '#include "dsplib/fpm_phasor.h"\n#include "dsplib/fpm_iir.h"')
    at = changed.index('/*\n * One table lookup with the quadrant sign applied.')
    helpers = '''/* The original loads the public writable tables. Negative cosine
 * quadrants address the sine table; negative sine quadrants address COEF_DC's
 * last three words and one zero padding word. Keep these as valid array
 * accesses while preserving the shared storage, rather than copied values. */
static short
phasor_cos_sign(int quad)
{
	return quad >= 0 ? FPM_cos_sign[quad] : FPM_sin_sign[quad + 4];
}

static short
phasor_sin_sign(int quad)
{
	if (quad >= 0)
		return FPM_sin_sign[quad];
	return quad == -1 ? 0 : COEF_DC[quad + 6];
}

'''
    changed = changed[:at] + helpers + changed[at:]
    changed = changed.replace('phasor_value(const unsigned short *table, const short *sign,', 'phasor_value(const unsigned short *table, short sign,')
    changed = changed.replace('* sign[FPM_PHASOR_SIGN_BELOW + quad]', '* sign')
    changed = changed.replace('FPM_cos_table, FPM_cos_sign_ext,', 'FPM_cos_table, phasor_cos_sign(quad),')
    changed = changed.replace('FPM_sin_table, FPM_sin_sign_ext,', 'FPM_sin_table, phasor_sin_sign(quad),')
    assert changed != source
    direct = source.replace('sign[FPM_PHASOR_SIGN_BELOW + quad]', 'sign[quad]')
    direct = direct.replace('FPM_cos_table, FPM_cos_sign_ext,', 'FPM_cos_table, FPM_cos_sign,')
    direct = direct.replace('FPM_sin_table, FPM_sin_sign_ext,', 'FPM_sin_table, FPM_sin_sign,')
    for name in ('FPM_cos_sign_ext', 'FPM_sin_sign_ext'):
        direct, count = re.subn(r'short ' + name + r'\[.*?\] = \{.*?\n\};', '', direct, flags=re.S)
        assert count == 1
    return {'baseline': source, 'shared-sign-storage': changed, 'direct-original-storage': direct}


if __name__ == '__main__':
    d.REV = '2feb98d8'
    d.SOURCE_PATHS = ('src/dsp/fpm_phasor.c',)
    d.OUT_NAME = 'stage-c-phasor-writable'
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dP')
    d.ALLOWED_GLOBAL_REMOVALS = lambda path, label: {'FPM_cos_sign_ext', 'FPM_sin_sign_ext'} if label == 'direct-original-storage' else set()
    d.variants = variants
    d.main()
