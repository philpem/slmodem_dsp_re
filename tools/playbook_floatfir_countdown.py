#!/usr/bin/env python3
"""Two complete-TU FloatFIR outer-loop countdown controls."""
import sys
import playbook_small_patterns as driver


def variants(path, source):
    start = source.index('FloatFIR::process(const float *in, float *out, unsigned int count)')
    end = source.index('\n}\n', start) + 2
    body = source[start:end]
    assert body.count('\tdo {') == 1
    assert body.count('\t} while (--count != 0);') == 1
    assert body.count('\tif (count == 0)\n\t\treturn;') == 1
    changed = body.replace('\tdo {', '\twhile (count-- != 0) {')
    changed = changed.replace('\t} while (--count != 0);', '\t}')
    return {'baseline': source, 'postdecrement':
            source[:start] + changed + source[end:]}


if __name__ == '__main__':
    assert '--domain' in sys.argv
    assert sys.argv[sys.argv.index('--domain') + 1].startswith(
        'https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-')
    driver.REV = '361ef919'
    driver.OUT_NAME = 'playbook-floatfir-countdown'
    driver.SOURCE_PATHS = ('src/dsp/FloatFIR.cpp',)
    driver.variants = variants
    driver.main()
