#!/usr/bin/env python3
"""Compare repeated owner reads against the blob's captured DTMF child."""
import sys
import playbook_small_patterns as driver


def variants(path, source):
    start, end, fn = driver.function(source, 'cid_get_strings')
    old = '\tif (ctx->mode != CID_MODE_FSK && ctx->mode != CID_MODE_FSK_DONE) {\n'
    assert fn.count(old) == 1 and fn.count('ctx->dtmf->digits[i]') == 1
    captured = fn.replace(old, old + '\t\tconst struct dtmf_rx *dtmf = ctx->dtmf;\n\n')
    captured = captured.replace('ctx->dtmf->digits[i]', 'dtmf->digits[i]')
    return {'baseline': source, 'captured-child': source[:start] + captured + source[end:]}


if __name__ == '__main__':
    assert '--domain' in sys.argv
    assert sys.argv[sys.argv.index('--domain') + 1].startswith(
        'https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-')
    driver.REV = '9565d51c'
    driver.OUT_NAME = 'playbook-cid-strings-capture'
    driver.SOURCE_PATHS = ('src/service/cidcore/cid.c',)
    driver.variants = variants
    driver.main()
