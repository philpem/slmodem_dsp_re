#!/usr/bin/env python3
"""Two observed initialization store-order classes after word counters."""
import playbook_small_patterns as driver
import playbook_v34det_counters as counters


def variants(path, source):
    original = counters.variants(path,source)['short-section-short-tap']
    results = {'baseline':source}
    for state_first in (False,True):
        for threshold_reverse in (False,True):
            text = original
            if state_first:
                assert text.count('\td->armed = 0;\n') == 1
                text = text.replace('\td->armed = 0;\n','')
                old = '\td->state = V34_DET_STATE_WARMUP;'
                assert text.count(old) == 1
                text = text.replace(old,old+'\n\td->armed = 0;')
            if threshold_reverse:
                old = '\td->thresh_hi = thresh_hi;\n\td->thresh_lo = thresh_lo;'
                assert text.count(old) == 1
                text = text.replace(old,'\td->thresh_lo = thresh_lo;\n\td->thresh_hi = thresh_hi;')
            label = 'shorts' + ('-state-first' if state_first else '') + ('-lo-first' if threshold_reverse else '')
            results[label] = text
    assert len(set(results.values())) == 5
    return results


if __name__ == '__main__':
    driver.REV = '97c06e4e'
    driver.OUT_NAME = 'playbook-v34det-stores'
    driver.SOURCE_PATHS = ('src/pump/v34/detector.c',)
    driver.variants = variants
    driver.main()
