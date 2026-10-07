#!/usr/bin/env python3
"""Remove the unwitnessed TX ratecfg owner, crossed with exact RX recovery."""
import playbook_small_patterns as d
from v34_rxrate_lifetime_reproduce import variants as rx_variants


def variants(path, source):
    cells = {}
    seed = rx_variants(path, source)['eager-1-common-1']
    for rx, tx in [(0, 0), (1, 0), (0, 1), (1, 1)]:
        text = seed if rx else source
        if tx:
            a, z, fn = d.function(text, 'VPcmV34GetCurrentTxBitRate')
            line = '\tconst struct v34_ratecfg *cfg = &obj->ratecfg;\n'
            assert fn.count(line) == fn.count('cfg->txbits') == 1
            fn = fn.replace(line, '').replace('cfg->txbits', 'obj->ratecfg.txbits')
            text = text[:a]+fn+text[z:]
        cells['baseline' if not (rx or tx) else f'rx-{rx}-tx-{tx}'] = text
    assert len(set(cells.values())) == 4
    return cells


if __name__ == '__main__':
    d.REV = 'b3665999'
    d.OUT_NAME = 'v34-txrate-owner'
    d.SOURCE_PATHS = ('src/pump/v34/VPcmV34Main.cpp',)
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dP')
    d.variants = variants
    d.main()
