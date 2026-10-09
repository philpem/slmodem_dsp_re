#!/usr/bin/env python3
"""Cross TX result flow and observed fallback owner on the exact RX seed."""
import playbook_small_patterns as d
from v34_rxrate_lifetime_reproduce import variants as rx_variants


def variants(path, source):
    cells = {'baseline': source}
    seed = rx_variants(path, source)['eager-1-common-1']
    for direct, result in [(0, 0), (1, 0), (0, 1), (1, 1)]:
        a, z, fn = d.function(seed, 'VPcmV34GetCurrentTxBitRate')
        if direct:
            line = '\tconst struct v34_ratecfg *cfg = &obj->ratecfg;\n'
            assert fn.count(line) == 1
            fn = fn.replace(line, '').replace('cfg->txbits', 'obj->ratecfg.txbits')
        if result:
            start = fn.index('\n\t/*')
            prefix = fn[:start]+'\n\tint rate;\n'
            fallback = ('obj->ratecfg.txbits' if direct else 'cfg->txbits')+' * (int)RATE_STEP'
            fn = prefix + ('\n\tif (obj->role != PCM_ROLE) {\n'
                '\t\tif ((unsigned)(obj->status - 1) <= 1) {\n'
                '\t\t\tV90Modulator *tx = sess->modem.modulator;\n'
                '\t\t\tif (tx->state != PCMTX_READY) {\n\t\t\t\trate = 0;\n'
                '\t\t\t} else {\n\t\t\t\tn = tx->bitsToSymbol->mapper->bitsPerFrame;\n'
                '\t\t\t\trate = (int)(unsigned)(n * 8000u * (1.0f / 6.0f) + 0.5f);\n'
                '\t\t\t}\n\t\t} else {\n\t\t\trate = '+fallback+';\n\t\t}\n'
                '\t} else if (obj->status == 2) {\n\t\tV92Modulator *tx = sess->v92modem.modulator;\n'
                '\t\tif (tx->phase != PCMTX_READY) {\n\t\t\trate = 0;\n'
                '\t\t} else {\n\t\t\tn = tx->bitsToSymbol->transmitter->K;\n'
                '\t\t\trate = (int)(unsigned)(n * 8000u * (1.0f / 12.0f) + 0.5f);\n'
                '\t\t}\n\t} else if (obj->status == 3) {\n\t\trate = K56FLEX_TX_BITRATE;\n'
                '\t} else {\n\t\trate = '+fallback+';\n\t}\n\treturn rate;\n}')
        cells[f'direct-{direct}-result-{result}'] = seed[:a]+fn+seed[z:]
    assert len(set(cells.values())) == 5
    return cells


if __name__ == '__main__':
    d.REV = 'b3665999'
    d.OUT_NAME = 'v34-txrate-result'
    d.SOURCE_PATHS = ('src/pump/v34/VPcmV34Main.cpp',)
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dP')
    d.variants = variants
    d.main()
