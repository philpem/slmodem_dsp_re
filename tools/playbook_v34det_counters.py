#!/usr/bin/env python3
"""Cross two observed word-sized detector initialization counters."""
import playbook_small_patterns as driver


def variants(path, source):
    start,end,body = driver.function(source,'detectorinit')
    old = '\tint section, tap;'
    assert body.count(old) == 1
    results = {}
    for section in ('int','short'):
        for tap in ('int','short'):
            label = 'baseline' if section == tap == 'int' else section + '-section-' + tap + '-tap'
            decl = '\t' + section + ' section;\n\t' + tap + ' tap;'
            text = body if label == 'baseline' else body.replace(old,decl)
            results[label] = source[:start]+text+source[end:]
    assert len(set(results.values())) == 4
    return results


if __name__ == '__main__':
    driver.REV = '97c06e4e'
    driver.OUT_NAME = 'playbook-v34det-counters'
    driver.SOURCE_PATHS = ('src/pump/v34/detector.c',)
    driver.variants = variants
    driver.main()
