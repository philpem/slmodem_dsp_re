#!/usr/bin/env python3
"""Complete-TU positive diagnostic count guard preserving the nonpositive return boundary."""
import playbook_small_patterns as driver


def variants(path, source):
    start, end, body = driver.function(source, 'FSE_getdiag')
    begin = body.index('\t\tfor (i = 0;', body.index('\tcase 1:'))
    finish = body.index('\t\treturn n;', begin)
    block = body[begin:finish]
    guarded = '\t\tif (n > 0) {\n' + ''.join('\t'+line+'\n' for line in block.splitlines()) + '\t\t}\n'
    text = body[:begin] + guarded + body[finish:]
    return {'baseline':source, 'positive-guard':source[:start]+text+source[end:]}


if __name__ == '__main__':
    driver.REV = '70bf8bc8'
    driver.OUT_NAME = 'playbook-fse-getdiag-guard'
    driver.SOURCE_PATHS = ('src/dsp/fpm_fse.c',)
    driver.variants = variants
    driver.main()
