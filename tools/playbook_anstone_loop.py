#!/usr/bin/env python3
"""Four complete-TU answer-tone countdown/cursor source controls."""
import playbook_small_patterns as driver


def variants(path, source):
    start, end, fn = driver.function(source, 'GenerateAnsTone')
    old = '\t\tint i;\n\n\t\tfor (i = 0; i < count; i++)\n\t\t\tout[i] = 0;'
    assert fn.count(old) == 1
    forms = {
        'baseline': old,
        'countdown': ('\t\tint i, remaining = count;\n\n'
                      '\t\tfor (i = 0; remaining > 0; --remaining, ++i)\n\t\t\tout[i] = 0;'),
        'cursor': '\t\tint i;\n\n\t\tfor (i = 0; i < count; i++)\n\t\t\t*out++ = 0;',
        'both': ('\t\tint remaining = count;\n\n'
                 '\t\twhile (remaining > 0) {\n\t\t\t*out++ = 0;\n\t\t\t--remaining;\n\t\t}')}
    return {label: source[:start] + fn.replace(old, body) + source[end:]
            for label, body in forms.items()}


if __name__ == '__main__':
    driver.REV = '9a16b620'
    driver.OUT_NAME = 'playbook-anstone-loop'
    driver.SOURCE_PATHS = ('src/pump/v32/v32anstone.c',)
    driver.variants = variants
    driver.main()
