#!/usr/bin/env python3
"""Three complete-TU adopting Resampler history publication controls."""
import playbook_small_patterns as driver


def variants(path, source):
    start = source.index('Resampler::Resampler(unsigned int nPhases, float scale, unsigned int nTaps,')
    end = source.index('\n}\n', start) + 2
    fn = source[start:end]
    assert 'float *bank' in fn
    old = '\thistory = 0;\n\tif (historyLen)\n\t\thistory = (float *)sysdep_malloc(historyLen * sizeof(float));'
    assert fn.count(old) == 1
    local = '\tfloat *allocated = 0;\n\tif (historyLen)\n\t\tallocated = (float *)sysdep_malloc(historyLen * sizeof(float));\n\thistory = allocated;'
    expression = '\thistory = historyLen ? (float *)sysdep_malloc(historyLen * sizeof(float)) : 0;'
    cells = {'baseline': source}
    for label, body in [('local-publish', local), ('expression-publish', expression)]:
        cells[label] = source[:start] + fn.replace(old, body) + source[end:]
    assert len(cells) == len(set(cells.values())) == 3
    return cells


if __name__ == '__main__':
    driver.REV = 'b550e9c6'
    driver.OUT_NAME = 'playbook-resampler-history'
    driver.SOURCE_PATHS = ('src/pump/v90/Resampler.cpp',)
    driver.variants = variants
    driver.main()
