#!/usr/bin/env python3
"""Bounded local-output normalization helper crossed with word index."""
import playbook_small_patterns as driver


def variants(path, source):
    start, end, body = driver.function(source, 'FPM_div_32')
    old = "\t/* Left-normalise until the top bit is set, counting the shifts. */\n\twhile ((int)denom >= 0) {\n\t\tdenom += denom;\n\t\tcount++;\n\t}\n\n\tmantissa = (unsigned short)(denom >> 16);"
    assert body.count(old) == 1
    helper = "static inline void\nnormalize32(unsigned int denom, unsigned short *mantissa, unsigned short *count)\n{\n\tint n = 0;\n\t*count = 0;\n\twhile ((int)denom >= 0) {\n\t\tdenom += denom;\n\t\tn++;\n\t}\n\t*count = (unsigned short)n;\n\t*mantissa = (unsigned short)(denom >> 16);\n}\n\n"
    results = {}
    for factoring in (False, True):
        for word in (False, True):
            text = body
            if factoring:
                text = text.replace('unsigned short count = 0;', 'unsigned short count;').replace(old, '\tnormalize32(denom, &mantissa, &count);')
            if word:
                assert text.count('\tint index;') == 1
                text = text.replace('\tint index;', '\tunsigned short index;')
            label = ('helper' if factoring else 'baseline') + ('-word-index' if word else '')
            assert source[:start].endswith('int\n')
            prefix = source[:start - 4] + helper + 'int\n' if factoring else source[:start]
            results[label] = prefix + text + source[end:]
    assert len(set(results.values())) == 4
    return results


if __name__ == '__main__':
    driver.REV = '397211df'
    driver.OUT_NAME = 'playbook-div32-normalize'
    driver.SOURCE_PATHS = ('src/dsp/fpm_div32.c',)
    driver.variants = variants
    driver.main()
