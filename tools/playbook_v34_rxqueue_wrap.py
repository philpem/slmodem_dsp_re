#!/usr/bin/env python3
"""Discriminate receive-queue direct wrap from the retained cursor helper."""
import sys
import playbook_small_patterns as driver
import playbook_v34_rxqueue as counter


def variants(path, source):
    combined = counter.variants(path, source)['counter-short_output-walk_read-half']
    start, end, fn = driver.function(combined, 'rxreadqueue')
    old = '\t\tp = q_next(q, p, V34_RXQ_END);'
    assert fn.count(old) == 1
    direct = combined[:start] + fn.replace(
        old, '\t\tif ((char *)p >= (char *)q + V34_RXQ_END)\n\t\t\tp = q->ring;') + combined[end:]
    return {'baseline': source, 'combined-control': combined, 'direct-wrap': direct}


if __name__ == '__main__':
    assert '--domain' in sys.argv
    assert sys.argv[sys.argv.index('--domain') + 1].startswith(
        'https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-')
    driver.REV = '9565d51c'
    driver.OUT_NAME = 'playbook-v34-rxqueue-wrap'
    driver.SOURCE_PATHS = ('src/pump/v34/V34RX.c',)
    driver.variants = variants
    driver.main()
